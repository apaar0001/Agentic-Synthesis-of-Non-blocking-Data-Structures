"""
cpp_extended_codebleu.py — Extended CodeBLEU Benchmark Scorer for C++ Lock-Free DS
===================================================================================

4-Layer scoring model adapted for C++:
  A + B = MULTIPLIERS (if either fails → combined = 0.0, still logged separately)
  C     = max CodeBLEU over ground-truth pool (weight 0.35)
  D1    = annotation JSON criterion match     (weight 0.20)
  D2    = CAS/Atomic concurrency primitives   (weight 0.15)
  D3    = LLM-as-judge (YES/NO)              (weight 0.15)
  D4    = structural patterns sub-bundle      (weight 0.15)
           D4a: CAS-loop retry pattern
           D4b: include correctness
           D4c: identifier semantic overlap

USAGE:
  python cpp_extended_codebleu.py --file ConcurrentDataStructure.hpp --ds linked_list
  python cpp_extended_codebleu.py --dir results_cpp/ --no-llm-judge

Requirements: 
  - 4.7.1, 4.7.2 (Layers A and B - test multipliers)
  - 4.7.3, 4.10.3, 4.10.4 (Layer C - multi-ref CodeBLEU)
  - 4.7.4 (Layer D1 - annotation)
  - 4.7.5 (Layer D2 - concurrency primitives)
  - 4.7.6 (Layer D3 - LLM judge)
  - 4.7.7 (Layer D4 - structural patterns)
  - 4.7.8, 4.7.9 (Combined score calculation)
"""
from __future__ import annotations

import os
import re
import sys
import json
import math
import argparse
import traceback
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple

# ── Optional dependencies ──────────────────────────────────────────────────────

try:
    from codebleu import calc_codebleu   # type: ignore
    _CODEBLEU_AVAILABLE = True
except ImportError:
    _CODEBLEU_AVAILABLE = False
    print("[WARN] 'codebleu' package not installed — Layer C will use BLEU-4 fallback.")

try:
    from tqdm import tqdm                # type: ignore
    _TQDM_AVAILABLE = True
except ImportError:
    _TQDM_AVAILABLE = False

try:
    from langchain_openai import ChatOpenAI
    _LLM_AVAILABLE = True
except ImportError:
    _LLM_AVAILABLE = False
    print("[WARN] 'langchain-openai' not installed — Layer D3 (LLM judge) skipped.")

_PROJECT_ROOT = Path(__file__).resolve().parent.parent

# =============================================================================
# ── Constants & Configuration ─────────────────────────────────────────────────
# =============================================================================

# Layers A and B are MULTIPLIERS — not part of the weighted sum.
# If either is 0.0 the combined score is forced to 0.0.
# Weights below apply only to Layers C and D (must sum to 1.0).
DEFAULT_WEIGHTS: Dict[str, float] = {
    "multi_ref_cb":        0.35,   # Layer C  : max CodeBLEU over pool
    "annotation":          0.20,   # Layer D1 : annotation JSON criterion match
    "concurrency":         0.15,   # Layer D2 : CAS/Atomic primitives
    "llm_judge":           0.15,   # Layer D3 : LLM-as-judge
    "structural_patterns": 0.15,   # Layer D4 : CAS-loop + include + identifier vocab
}

# Module-level caches
_categorized_cache: Dict[str, List[Tuple[str, str]]] = {}
_annotation_cache:  Dict[str, Optional[Dict]]         = {}


# =============================================================================
# Layer C — Multi-Ref CodeBLEU (max score over ground truth pool)
# =============================================================================

def score_multi_ref_codebleu(
    code: str,
    ds_name: str,
) -> Tuple[float, str]:
    """
    Layer C — compute CodeBLEU against all ground truth structures and take max.
    
    Uses tree-sitter-cpp for AST-based matching. Falls back to BLEU-4 if tree-sitter fails.
    
    Requirements: 4.7.3, 4.10.3, 4.10.4
    
    Args:
        code: Generated C++ code to evaluate
        ds_name: Data structure name for loading ground truth pool
        
    Returns:
        Tuple of (max_score, detail_message)
    """
    # Import ground truth loader
    try:
        from cpp_codebleu.cpp_ground_truth import load_ground_truth
    except ImportError:
        return 0.0, "Ground truth loader not available"
    
    # Load ground truth pool
    try:
        ground_truth_pool = load_ground_truth(ds_name)
    except Exception as exc:
        return 0.0, f"Failed to load ground truth: {exc}"
    
    if not ground_truth_pool:
        return 0.0, f"No ground truth structures found for {ds_name}"
    
    # Compute CodeBLEU for each reference
    max_score = 0.0
    best_ref = ""
    scores = []
    used_fallback = False
    
    for ref_name, ref_code in ground_truth_pool:
        try:
            if _CODEBLEU_AVAILABLE:
                # Try CodeBLEU with tree-sitter-cpp
                try:
                    result = calc_codebleu(
                        references=[ref_code],
                        predictions=[code],
                        lang="cpp",
                        weights=(0.25, 0.25, 0.25, 0.25),
                        tokenizer=None
                    )
                    score = result.get("codebleu", 0.0)
                except (ImportError, Exception) as tree_err:
                    # Fallback to BLEU-4 if tree-sitter fails
                    # Use simple n-gram matching without AST
                    from collections import Counter
                    import re
                    
                    def tokenize(text):
                        """Simple tokenization for BLEU-4 fallback."""
                        # Remove comments
                        text = re.sub(r'//.*?$', '', text, flags=re.MULTILINE)
                        text = re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)
                        # Tokenize
                        tokens = re.findall(r'\w+|[^\w\s]', text.lower())
                        return tokens
                    
                    def compute_bleu_4(reference, prediction):
                        """Compute BLEU-4 score (simple n-gram overlap)."""
                        ref_tokens = tokenize(reference)
                        pred_tokens = tokenize(prediction)
                        
                        if not pred_tokens:
                            return 0.0
                        
                        # Compute n-gram precision for n=1,2,3,4
                        precisions = []
                        for n in range(1, 5):
                            ref_ngrams = Counter([tuple(ref_tokens[i:i+n]) for i in range(len(ref_tokens)-n+1)])
                            pred_ngrams = Counter([tuple(pred_tokens[i:i+n]) for i in range(len(pred_tokens)-n+1)])
                            
                            overlap = sum((ref_ngrams & pred_ngrams).values())
                            total = sum(pred_ngrams.values())
                            
                            if total == 0:
                                precisions.append(0.0)
                            else:
                                precisions.append(overlap / total)
                        
                        # Geometric mean of precisions
                        if all(p > 0 for p in precisions):
                            import math
                            bleu = math.exp(sum(math.log(p) for p in precisions) / 4)
                        else:
                            bleu = 0.0
                        
                        # Brevity penalty
                        bp = 1.0
                        if len(pred_tokens) < len(ref_tokens):
                            bp = math.exp(1 - len(ref_tokens) / len(pred_tokens))
                        
                        return bp * bleu
                    
                    score = compute_bleu_4(ref_code, code)
                    used_fallback = True
            else:
                # No codebleu package - use simple token overlap as fallback
                from collections import Counter
                import re
                
                # Simple tokenization
                def tokenize(text):
                    return re.findall(r'\w+|[^\w\s]', text.lower())
                
                ref_tokens = Counter(tokenize(ref_code))
                pred_tokens = Counter(tokenize(code))
                
                # Compute overlap
                overlap = sum((ref_tokens & pred_tokens).values())
                total = sum(pred_tokens.values())
                score = overlap / total if total > 0 else 0.0
                used_fallback = True
            
            scores.append((ref_name, score))
            
            if score > max_score:
                max_score = score
                best_ref = ref_name
                
        except Exception as exc:
            # Log error but continue with other references
            scores.append((ref_name, 0.0))
            continue
    
    # Format detail message
    fallback_note = " (BLEU-4 fallback)" if used_fallback else ""
    if max_score > 0:
        detail = f"Max CodeBLEU={max_score:.4f} vs {best_ref} (tested {len(scores)} refs){fallback_note}"
    else:
        detail = f"All CodeBLEU scores were 0.0 (tested {len(scores)} refs){fallback_note}"
    
    return max_score, detail


# =============================================================================
# Layer D1 — Annotation (pattern matching against annotation JSON)
# =============================================================================

def score_annotation(code: str, ds_name: str) -> Tuple[float, str]:
    """
    Layer D1 — score code against annotation JSON sidecar.

    Mirrors Java score_annotation_match exactly:
      Sub-score 1: atomic_class_match        (weight 0.25)
      Sub-score 2: required_keywords_match   (weight 0.35)
      Sub-score 3: forbidden_keywords_absent (weight 0.20)
      Sub-score 4: structural_invariants     (weight 0.20)
    Returns 0.5 neutral if no annotation found.

    Requirements: 4.7.4
    """
    try:
        from cpp_codebleu.cpp_annotation import score_cpp_annotation
    except ImportError:
        try:
            from cpp_annotation import score_cpp_annotation  # type: ignore
        except ImportError:
            return 0.5, "Annotation scorer not available"

    try:
        score, details = score_cpp_annotation(code, ds_name)
        return score, details.get("detail", f"score={score:.4f}")
    except Exception as exc:
        return 0.5, f"Annotation scoring failed: {exc}"


# =============================================================================
# Layer D2 — Concurrency Primitives (atomic operations and lock checks)
# =============================================================================

def score_concurrency_primitives(code: str) -> Tuple[float, str]:
    """
    Layer D2 — check for required concurrency primitives and forbidden patterns.
    
    Checks for:
    - Required: std::atomic, compare_exchange (CAS operations)
    - Forbidden: std::mutex, std::lock_guard, std::unique_lock
    
    Score = (positive_matches / total_positive) * (1 - negative_penalty)
    
    Requirements: 4.7.5
    
    Args:
        code: Generated C++ code to evaluate
        
    Returns:
        Tuple of (score, detail_message)
    """
    # Define required patterns (positive)
    positive_patterns = {
        'std::atomic': r'std::atomic\s*<',
        'compare_exchange': r'compare_exchange_(strong|weak)',
        'load_store': r'\.(load|store)\s*\(',
        'memory_ordering': r'memory_order_(acquire|release|acq_rel|seq_cst|relaxed)',
    }
    
    # Define forbidden patterns (negative)
    negative_patterns = {
        'std::mutex': r'std::mutex',
        'std::lock_guard': r'std::lock_guard',
        'std::unique_lock': r'std::unique_lock',
        'std::shared_lock': r'std::shared_lock',
    }
    
    # Check positive patterns
    positive_matches = 0
    positive_details = []
    for name, pattern in positive_patterns.items():
        if re.search(pattern, code):
            positive_matches += 1
            positive_details.append(f"{name}✓")
        else:
            positive_details.append(f"{name}✗")
    
    # Check negative patterns
    negative_matches = 0
    negative_details = []
    for name, pattern in negative_patterns.items():
        if re.search(pattern, code):
            negative_matches += 1
            negative_details.append(f"{name}✗")
    
    # Compute score
    total_positive = len(positive_patterns)
    positive_ratio = positive_matches / total_positive if total_positive > 0 else 0.0
    
    # Apply penalty for forbidden patterns
    # Each forbidden pattern reduces score by 0.3
    negative_penalty = min(0.9, negative_matches * 0.3)
    
    score = positive_ratio * (1.0 - negative_penalty)
    score = max(0.0, min(1.0, score))
    
    # Build detail message
    pos_str = ", ".join(positive_details)
    neg_str = f", forbidden: {', '.join(negative_details)}" if negative_details else ""
    detail = f"Concurrency: {score:.3f} ({pos_str}{neg_str})"
    
    return score, detail


# =============================================================================
# Layer D3 — LLM Judge (optional YES/NO verdict)
# =============================================================================

def score_llm_judge(code: str, ds_name: str, use_llm: bool = True) -> Tuple[float, str, str]:
    """
    Layer D3 — ask LLM to judge if code is correct lock-free implementation.
    
    Prompts LLM with: "Is this correct lock-free C++ <DS>?"
    Parses YES/NO response.
    Score: 1.0 if YES, 0.0 if NO, 0.5 if SKIP
    
    Requirements: 4.7.6
    
    Args:
        code: Generated C++ code to evaluate
        ds_name: Data structure name for context
        use_llm: Whether to actually call LLM (if False, returns SKIP)
        
    Returns:
        Tuple of (score, verdict, reason)
        - score: 1.0 (YES), 0.0 (NO), or 0.5 (SKIP)
        - verdict: "YES", "NO", or "SKIP"
        - reason: Explanation from LLM or skip reason
    """
    if not use_llm:
        return 0.5, "SKIP", "LLM judge disabled by user"
    
    if not _LLM_AVAILABLE:
        return 0.5, "SKIP", "langchain-openai not installed"
    
    # Get LLM configuration from environment
    model_name = os.environ.get("NVIDIA_NIM_MODEL", "meta/llama-3.1-70b-instruct")
    api_key = os.environ.get("NVIDIA_NIM_API_KEY", "")
    base_url = os.environ.get("NVIDIA_BASE_URL", "https://integrate.api.nvidia.com/v1")
    
    if not api_key:
        return 0.5, "SKIP", "NVIDIA_NIM_API_KEY not set"
    
    try:
        # Initialize LLM
        llm = ChatOpenAI(
            model=model_name,
            api_key=api_key,
            base_url=base_url,
            temperature=0.0,
        )
        
        # Format DS name for prompt
        ds_display = ds_name.replace("_", " ").title()
        
        # Create prompt
        prompt = f"""You are a C++ concurrency expert. Analyze the following C++ code and determine if it is a correct lock-free implementation of a {ds_display}.

A correct lock-free implementation must:
1. Use std::atomic for shared state
2. Use compare_exchange (CAS) operations for synchronization
3. NOT use locks (std::mutex, std::lock_guard, etc.)
4. Be thread-safe and provide progress guarantees
5. Handle memory ordering correctly

Code to analyze:
```cpp
{code[:3000]}  // Truncated to first 3000 chars for efficiency
```

Answer with ONLY "YES" or "NO" followed by a brief reason (one sentence).
Format: YES: <reason> OR NO: <reason>"""
        
        # Call LLM
        response = llm.invoke(prompt)
        response_text = response.content.strip()
        
        # Parse response
        if response_text.upper().startswith("YES"):
            verdict = "YES"
            score = 1.0
            # Extract reason after "YES:"
            reason = response_text[3:].strip().lstrip(":").strip()
            if not reason:
                reason = "LLM judged code as correct"
        elif response_text.upper().startswith("NO"):
            verdict = "NO"
            score = 0.0
            # Extract reason after "NO:"
            reason = response_text[2:].strip().lstrip(":").strip()
            if not reason:
                reason = "LLM judged code as incorrect"
        else:
            # Ambiguous response
            verdict = "SKIP"
            score = 0.5
            reason = f"Ambiguous LLM response: {response_text[:100]}"
        
        return score, verdict, reason
        
    except Exception as exc:
        return 0.5, "SKIP", f"LLM judge error: {str(exc)[:100]}"


# =============================================================================
# Layer D4 — Structural Patterns (CAS-loop, includes, identifier vocab)
# =============================================================================

def score_structural_patterns(code: str, ds_name: str) -> Tuple[float, str]:
    """
    Layer D4 — check for structural patterns in lock-free code.
    
    Three sub-components:
    - D4a: CAS-loop retry pattern (while + CAS)
    - D4b: Include correctness (<atomic>, no <mutex>)
    - D4c: Identifier vocabulary (head, tail, next, etc.)
    
    Computes weighted sub-score.
    
    Requirements: 4.7.7
    
    Args:
        code: Generated C++ code to evaluate
        ds_name: Data structure name for context
        
    Returns:
        Tuple of (score, detail_message)
    """
    # Sub-weights for D4 components (must sum to 1.0)
    d4_weights = {
        'cas_loop': 0.30,      # D4a: CAS-loop retry pattern
        'includes': 0.20,      # D4b: Include correctness
        'identifiers': 0.25,   # D4c: Identifier vocabulary
        'marked_ptrs': 0.25,   # D4d: Marked pointer patterns
    }
    
    # ── D4a: CAS-loop retry pattern ────────────────────────────────────────────
    # Look for patterns like:
    #   while (!ptr->compare_exchange_weak(...)) { ... }
    #   do { ... } while (!compare_exchange_strong(...))
    cas_loop_patterns = [
        r'while\s*\([^)]*compare_exchange_(weak|strong)',  # while with CAS
        r'do\s*\{[^}]*\}\s*while\s*\([^)]*compare_exchange',  # do-while with CAS
        r'for\s*\([^)]*compare_exchange',  # for loop with CAS
    ]
    
    cas_loop_found = any(re.search(pattern, code, re.DOTALL) for pattern in cas_loop_patterns)
    d4a_score = 1.0 if cas_loop_found else 0.0
    
    # ── D4b: Include correctness ───────────────────────────────────────────────
    # Required includes: <atomic>
    # Forbidden includes: <mutex>
    has_atomic = bool(re.search(r'#include\s*<atomic>', code))
    has_mutex = bool(re.search(r'#include\s*<mutex>', code))
    
    if has_atomic and not has_mutex:
        d4b_score = 1.0
    elif has_atomic and has_mutex:
        d4b_score = 0.5  # Has atomic but also has mutex (mixed)
    else:
        d4b_score = 0.0  # Missing atomic or only has mutex
    
    # ── D4c: Identifier vocabulary ─────────────────────────────────────────────
    # Check for common identifiers in concurrent data structures
    # Different DS types have different expected identifiers
    identifier_sets = {
        'linked_list': ['head', 'tail', 'next', 'node', 'prev'],
        'skiplist': ['head', 'tail', 'next', 'level', 'forward', 'node'],
        'bst': ['root', 'left', 'right', 'node', 'parent', 'child'],
        'hash_table': ['bucket', 'table', 'hash', 'size', 'capacity', 'entry'],
    }
    
    expected_identifiers = identifier_sets.get(ds_name, ['node', 'next', 'data'])
    
    # Count how many expected identifiers are present (case-insensitive)
    code_lower = code.lower()
    found_identifiers = sum(1 for ident in expected_identifiers if ident in code_lower)
    
    # Score based on percentage of expected identifiers found
    d4c_score = found_identifiers / len(expected_identifiers) if expected_identifiers else 0.5
    
    # ── D4d: Marked pointer patterns ───────────────────────────────────────────
    # Detect C++-specific lock-free patterns: bit manipulation for marked pointers,
    # reinterpret_cast for pointer tagging, and explicit memory ordering
    marked_ptr_patterns = [
        r'reinterpret_cast\s*<\s*uintptr_t\s*>',   # pointer-to-int cast for mark bits
        r'[&|]\s*1[Ll]?\b',                         # bit manipulation (& 1L, | 1L)
        r'(is_marked|get_marked|get_unmarked)',      # marked pointer helper functions
    ]
    marked_ptr_found = sum(1 for p in marked_ptr_patterns if re.search(p, code))
    d4d_score = min(1.0, marked_ptr_found / 2.0)  # 2 of 3 patterns = full score
    
    # ── Compute weighted D4 score ──────────────────────────────────────────────
    d4_score = (
        d4a_score * d4_weights['cas_loop'] +
        d4b_score * d4_weights['includes'] +
        d4c_score * d4_weights['identifiers'] +
        d4d_score * d4_weights['marked_ptrs']
    )
    
    # Build detail message
    d4a_str = "CAS-loop✓" if cas_loop_found else "CAS-loop✗"
    d4b_str = f"includes✓" if d4b_score == 1.0 else (f"includes~" if d4b_score == 0.5 else "includes✗")
    d4c_str = f"vocab:{found_identifiers}/{len(expected_identifiers)}"
    d4d_str = f"marked:{marked_ptr_found}/3" if marked_ptr_found > 0 else "marked✗"
    
    detail = f"Structural: {d4_score:.3f} ({d4a_str}, {d4b_str}, {d4c_str}, {d4d_str})"
    
    return d4_score, detail


# =============================================================================
# Layer A — Consistency Test (multiplier: 1.0 pass / 0.0 fail)
# =============================================================================

def score_consistency_test(test_results: Optional[Dict]) -> Tuple[float, str]:
    """
    Layer A — hard binary multiplier.
    1.0 → consistency_status == 'pass'
    0.0 → anything else (logged but not weighted)
    
    Requirements: 4.7.1
    
    Args:
        test_results: Dictionary containing test results with 'consistency_status' key
        
    Returns:
        Tuple of (score, detail_message)
    """
    if not test_results:
        return 0.0, "test_results not provided"
    
    status = str(test_results.get("consistency_status", "none"))
    
    if status == "pass":
        return 1.0, "Consistency test PASSED ✓"
    
    return 0.0, f"Consistency test: {status} ✗"


# =============================================================================
# Layer B — Non-Blocking Test (multiplier: 1.0 lock-free / 0.0 otherwise)
# =============================================================================

def score_nonblocking_test(test_results: Optional[Dict]) -> Tuple[float, str]:
    """
    Layer B — hard binary multiplier (semantic result only).
    1.0 → lock_freedom_status == 'lock-free'
    0.0 → anything else (syntax result is diagnostic only, not scored)
    
    Requirements: 4.7.2
    
    Args:
        test_results: Dictionary containing test results with 'lock_freedom_status' key
        
    Returns:
        Tuple of (score, detail_message)
    """
    if not test_results:
        return 0.0, "test_results not provided"
    
    semantic = str(test_results.get("lock_freedom_status", "none"))
    syntax   = str(test_results.get("lock_syntax_status",  "unknown"))
    
    if semantic == "lock-free":
        return 1.0, f"Semantic=lock-free ✓ (syntax={syntax}, diagnostic only)"
    
    return 0.0, f"Semantic={semantic} ✗ (syntax={syntax})"


# =============================================================================
# ── Combined scorer (Layers A & B only for now) ──────────────────────────────
# =============================================================================

def evaluate_cpp_file(
    hpp_path: Path,
    ds_name: str,
    test_results: Optional[Dict[str, str]] = None,
    weights: Dict[str, float] = DEFAULT_WEIGHTS,
    use_llm_judge: bool = True,
    approach: str = "unknown",
) -> Dict[str, Any]:
    """
    Evaluate a C++ .hpp file against the 4-layer benchmark.
    
    This implementation includes:
      - Layers A & B: Test result multipliers (task 8.1)
      - Layer C: Multi-Ref CodeBLEU (task 8.2)
      - Layer D1: Annotation (task 8.3)
      - Layer D2: Concurrency primitives (task 8.4)
      - Layer D3: LLM Judge (task 8.5)
      - Layer D4: Structural patterns (task 8.6)
      - Combined score calculation (task 8.7)

    Layers A & B are MULTIPLIERS:
      - Still reported and logged (pass/fail) in results
      - If either is 0.0 → combined_score is forced to 0.0
      - They carry no weight in the weighted sum

    Layers C + D1 + D2 + D3 + D4 are WEIGHTED (must sum to 1.0).

    Parameters
    ----------
    hpp_path     : path to generated ConcurrentDataStructure.hpp
    ds_name      : 'linked_list' | 'skiplist' | 'bst' | 'hash_table'
    test_results : dict with consistency_status, lock_freedom_status, lock_syntax_status
    weights      : override DEFAULT_WEIGHTS (C + D layers only)
    use_llm_judge: run Layer D3 LLM-as-judge
    approach     : 'zero_shot' or 'translation' (for logging)
    
    Returns
    -------
    Dictionary containing all layer scores and combined score
    """
    result: Dict[str, Any] = {
        "file":      str(hpp_path),
        "ds_name":   ds_name,
        "approach":  approach,
        "timestamp": datetime.now().isoformat(),
        # Multipliers (A & B) — logged separately, not weighted
        "consistency_score":         0.0,
        "consistency_detail":        "",
        "nonblocking_score":         0.0,
        "nonblocking_detail":        "",
        # Weighted layers
        "multi_ref_cb_score":        0.0,
        "multi_ref_cb_detail":       "",
        "annotation_score":          0.0,
        "annotation_detail":         "",
        "concurrency_score":         0.0,
        "concurrency_detail":        "",
        "llm_judge_score":           0.5,
        "llm_judge_verdict":         "SKIP",
        "llm_judge_reason":          "",
        "structural_patterns_score": 0.0,
        "structural_patterns_detail":"",
        # Combined
        "combined_score":  0.0,
        "is_correct_ds":   False,
        "error":           "",
    }

    # Guard: check file exists
    if not hpp_path.exists():
        result["error"] = f"File not found: {hpp_path}"
        return result
    
    try:
        code = hpp_path.read_text(encoding="utf-8", errors="ignore")
    except Exception as exc:
        result["error"] = f"Cannot read file: {exc}"
        return result
    
    if len(code.strip()) < 50:
        result["error"] = "File is empty or too short"
        return result

    # ── Layer A: Consistency (multiplier) ──────────────────────────────────────
    a_score, a_detail = score_consistency_test(test_results)
    result["consistency_score"]  = round(a_score, 4)
    result["consistency_detail"] = a_detail

    # ── Layer B: Non-Blocking (multiplier) ─────────────────────────────────────
    b_score, b_detail = score_nonblocking_test(test_results)
    result["nonblocking_score"]  = round(b_score, 4)
    result["nonblocking_detail"] = b_detail

    # ── Layer C: Multi-Ref CodeBLEU ────────────────────────────────────────────
    c_score, c_detail = score_multi_ref_codebleu(code, ds_name)
    result["multi_ref_cb_score"]  = round(c_score, 4)
    result["multi_ref_cb_detail"] = c_detail

    # ── Layer D1: Annotation ───────────────────────────────────────────────────
    d1_score, d1_detail = score_annotation(code, ds_name)
    result["annotation_score"]  = round(d1_score, 4)
    result["annotation_detail"] = d1_detail

    # ── Layer D2: Concurrency Primitives ───────────────────────────────────────
    d2_score, d2_detail = score_concurrency_primitives(code)
    result["concurrency_score"]  = round(d2_score, 4)
    result["concurrency_detail"] = d2_detail

    # ── Layer D3: LLM Judge (optional) ─────────────────────────────────────────
    d3_score, d3_verdict, d3_reason = score_llm_judge(code, ds_name, use_llm_judge)
    result["llm_judge_score"]   = round(d3_score, 4)
    result["llm_judge_verdict"] = d3_verdict
    result["llm_judge_reason"]  = d3_reason

    # ── Layer D4: Structural Patterns ──────────────────────────────────────────
    d4_score, d4_detail = score_structural_patterns(code, ds_name)
    result["structural_patterns_score"]  = round(d4_score, 4)
    result["structural_patterns_detail"] = d4_detail
    
    # ── Weighted combination (C + D layers) ────────────────────────────────────
    # All layers (C, D1, D2, D3, D4) are now implemented
    active_weights = dict(weights)
    if not use_llm_judge:
        active_weights.pop("llm_judge", None)
        result["llm_judge_score"]   = 0.5  # neutral
        result["llm_judge_verdict"] = "SKIP"

    total_w = sum(active_weights.values())
    norm_w  = {k: v / total_w for k, v in active_weights.items()} if total_w > 0 else {}

    layer_scores = {
        "multi_ref_cb":        result["multi_ref_cb_score"],
        "annotation":          result["annotation_score"],
        "concurrency":         result["concurrency_score"],
        "llm_judge":           result["llm_judge_score"],
        "structural_patterns": result["structural_patterns_score"],
    }
    weighted_sum = sum(layer_scores[k] * norm_w.get(k, 0.0) for k in layer_scores)

    # ── Multiplier gate ────────────────────────────────────────────────────────
    if a_score == 0.0 or b_score == 0.0:
        combined = 0.0   # either multiplier failed → hard zero
    else:
        combined = weighted_sum

    result["combined_score"] = round(combined, 4)

    # Correctness: combined > 0.55 AND both multipliers passed
    result["is_correct_ds"] = (
        result["combined_score"] > 0.55
        and a_score == 1.0
        and b_score == 1.0
    )

    return result


# =============================================================================
# ── Batch helpers ─────────────────────────────────────────────────────────────
# =============================================================================

_DS_ALIASES = {
    "linked_list": ["linked_list", "linkedlist", "ll"],
    "skiplist":    ["skiplist", "skip_list"],
    "bst":         ["bst", "binary_search_tree", "binarysearchtree"],
    "hash_table":  ["hash_table", "hashtable", "ht"],
}


def _guess_ds_from_path(path: Path) -> str:
    """Guess data structure name from file path."""
    path_str = str(path).lower().replace("\\", "/")
    for ds, aliases in _DS_ALIASES.items():
        if any(a in path_str for a in aliases):
            return ds
    return "unknown"


def _guess_approach_from_path(path: Path) -> str:
    """Guess approach (zero_shot or translation) from file path."""
    path_str = str(path).lower()
    return "zero_shot" if ("zero_shot" in path_str or "zeroshot" in path_str) else "translation"


def _find_hpp_files(root: Path, ds_name: Optional[str] = None) -> List[Path]:
    """Find all C++ .hpp files to evaluate."""
    results = []
    for p in root.rglob("ConcurrentDataStructure.hpp"):
        results.append(p)
    for p in root.rglob("*.hpp"):
        if "Concurrent" in p.name and p not in results:
            results.append(p)
    return results


def _infer_additional_context(hpp_path: Path) -> Dict[str, str]:
    """Infer model, prompt_idx, sample_idx from file path."""
    parts = hpp_path.parts
    ctx   = {"model": "unknown", "prompt_idx": "?", "sample_idx": "?"}
    for i, part in enumerate(parts):
        if re.match(r"sample_\d+", part):
            m = re.match(r"sample_(\d+)", part)
            if m:
                ctx["sample_idx"] = m.group(1)
        elif part in ("results_cpp", "generated_code_cpp"):
            if i + 1 < len(parts):
                ctx["model"] = parts[i + 1]
    return ctx


CSV_HEADERS_BATCH = [
    "file", "ds_name", "approach", "model", "sample_idx",
    "consistency_score", "nonblocking_score",           # multipliers
    "multi_ref_cb_score", "annotation_score",
    "concurrency_score", "llm_judge_score",
    "structural_patterns_score",
    "combined_score", "is_correct_ds",
    "llm_judge_verdict", "llm_judge_reason",
    "multi_ref_cb_detail", "annotation_detail",
    "concurrency_detail", "structural_patterns_detail",
    "error",
]


def _result_to_csv_row(r: Dict[str, Any], ctx: Dict[str, str]) -> List[str]:
    """Convert result dictionary to CSV row."""
    def _c(v):
        return str(v).replace(",", ";").replace("\n", " ").replace("\r", "")
    return [
        _c(r.get("file", "")), _c(r.get("ds_name", "")), _c(r.get("approach", "")),
        _c(ctx.get("model", "unknown")), _c(ctx.get("sample_idx", "?")),
        _c(r.get("consistency_score", 0.0)), _c(r.get("nonblocking_score", 0.0)),
        _c(r.get("multi_ref_cb_score", 0.0)), _c(r.get("annotation_score", 0.0)),
        _c(r.get("concurrency_score", 0.0)), _c(r.get("llm_judge_score", 0.0)),
        _c(r.get("structural_patterns_score", 0.0)),
        _c(r.get("combined_score", 0.0)), _c(r.get("is_correct_ds", False)),
        _c(r.get("llm_judge_verdict", "")), _c(r.get("llm_judge_reason", "")),
        _c(r.get("multi_ref_cb_detail", "")), _c(r.get("annotation_detail", "")),
        _c(r.get("concurrency_detail", "")), _c(r.get("structural_patterns_detail", "")),
        _c(r.get("error", "")),
    ]


def _print_summary(results: List[Dict[str, Any]]) -> None:
    """Print summary of evaluation results."""
    total   = len(results)
    correct = sum(1 for r in results if r.get("is_correct_ds"))
    avg_cb  = (sum(r.get("combined_score", 0) for r in results) / total) if total else 0
    print("\n" + "=" * 80)
    print(f"C++ EXTENDED CODEBLEU SUMMARY  |  {total} files evaluated")
    print("=" * 80)
    print(f"{'File':<50} {'DS':<20} {'Combined':>8} {'Correct?':>9}")
    print("-" * 80)
    for r in results:
        fname = Path(r["file"]).name
        ds    = r.get("ds_name", "?")[:18]
        score = f"{r['combined_score']:.3f}"
        ok    = "✓" if r.get("is_correct_ds") else "✗"
        a_ok  = "A✓" if r.get("consistency_score") == 1.0 else "A✗"
        b_ok  = "B✓" if r.get("nonblocking_score") == 1.0 else "B✗"
        print(f"{fname:<50} {ds:<20} {score:>8} {ok:>4} {a_ok} {b_ok}")
    print("-" * 80)
    print(f"{'TOTAL':<50} {'':<20} {avg_cb:.3f} avg  {correct}/{total} correct")
    print("=" * 80 + "\n")


# =============================================================================
# ── CLI entry point ───────────────────────────────────────────────────────────
# =============================================================================

def _load_env() -> None:
    """Load environment variables from .env file."""
    env_path = _PROJECT_ROOT / ".env"
    if env_path.exists():
        for raw in env_path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            k = k.strip(); v = v.strip().strip('"').strip("'")
            if k and v and not os.environ.get(k):
                os.environ[k] = v


def main() -> None:
    """Main CLI entry point."""
    _load_env()

    parser = argparse.ArgumentParser(
        description="Extended CodeBLEU — Semantic Correctness Checker for C++ Lock-Free DS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    mode_group = parser.add_mutually_exclusive_group(required=True)
    mode_group.add_argument("--file", type=Path, help="Evaluate a single .hpp file")
    mode_group.add_argument("--dir",  type=Path, help="Evaluate all .hpp files in directory")
    parser.add_argument("--ds", default=None, choices=list(_DS_ALIASES.keys()),
                       help="Data structure name (required for --file mode)")
    parser.add_argument("--output", type=Path, default=None,
                       help="Output CSV file path")
    parser.add_argument("--json-output", type=Path, default=None,
                       help="Output JSON file path")
    parser.add_argument("--no-llm-judge", action="store_true",
                       help="Skip LLM judge (Layer D3)")
    parser.add_argument("--model", default=None,
                       help="Override LLM model")
    args = parser.parse_args()

    if args.model:
        os.environ["NVIDIA_NIM_MODEL"] = args.model
    use_llm = not args.no_llm_judge

    files_to_eval: List[Tuple[Path, str, str]] = []
    if args.file:
        if args.ds is None:
            parser.error("--ds is required when using --file mode")
        files_to_eval.append((args.file, args.ds, _guess_approach_from_path(args.file)))
    else:
        if not args.dir.exists():
            print(f"[ERROR] Directory not found: {args.dir}")
            sys.exit(1)
        for hpp_file in _find_hpp_files(args.dir, args.ds):
            ds       = args.ds or _guess_ds_from_path(hpp_file)
            approach = _guess_approach_from_path(hpp_file)
            files_to_eval.append((hpp_file, ds, approach))

    if not files_to_eval:
        print("[ERROR] No files to evaluate.")
        sys.exit(1)

    print(f"\n[C++ Extended CodeBLEU] Evaluating {len(files_to_eval)} file(s)...")
    print(f"  LLM judge : {'ON' if use_llm else 'OFF'}")
    print(f"  CodeBLEU  : {'ON' if _CODEBLEU_AVAILABLE else 'OFF (BLEU-4 fallback)'}")
    print(f"  All layers (A, B, C, D1-D4) are fully implemented.\n")

    all_results: List[Dict[str, Any]] = []
    iterator = files_to_eval
    if _TQDM_AVAILABLE:
        iterator = tqdm(files_to_eval, desc="Evaluating")  # type: ignore

    for hpp_path, ds_name, approach in iterator:
        try:
            result = evaluate_cpp_file(hpp_path, ds_name, use_llm_judge=use_llm, approach=approach)
        except Exception as exc:
            result = {
                "file": str(hpp_path), "ds_name": ds_name, "approach": approach,
                "combined_score": 0.0, "is_correct_ds": False,
                "error": f"Exception: {exc}\n{traceback.format_exc()[:300]}",
            }
        all_results.append(result)

    _print_summary(all_results)

    output_csv = args.output or Path(f"cpp_semantic_results_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
    with open(output_csv, "w", encoding="utf-8") as f:
        f.write(",".join(CSV_HEADERS_BATCH) + "\n")
        for r in all_results:
            ctx = _infer_additional_context(Path(r["file"]))
            f.write(",".join(_result_to_csv_row(r, ctx)) + "\n")
    print(f"CSV results written to: {output_csv}")

    if args.json_output:
        with open(args.json_output, "w", encoding="utf-8") as f:
            json.dump(all_results, f, indent=2, default=str)
        print(f"JSON results written to: {args.json_output}")


if __name__ == "__main__":
    main()
