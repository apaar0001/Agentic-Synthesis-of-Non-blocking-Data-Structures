"""
extended_codebleu.py — Extended CodeBLEU Benchmark Scorer for Lock-Free DS
===========================================================================

4-Layer scoring model:
  A + B = MULTIPLIERS (if either fails → combined = 0.0, still logged separately)
  C     = max CodeBLEU over ground-truth pool (weight 0.35)
  D1    = annotation JSON criterion match     (weight 0.20)
  D2    = CAS/Atomic concurrency primitives   (weight 0.15)
  D3    = LLM-as-judge (YES/NO)              (weight 0.15)
  D4    = structural patterns sub-bundle      (weight 0.15)
           D4a: CAS-loop retry pattern
           D4b: import correctness
           D4c: identifier semantic overlap

USAGE:
  python extended_codebleu.py --file ConcurrentDataStructure.java --ds linked_list
  python extended_codebleu.py --dir results_zero_shot/ --no-llm-judge
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

_PROJECT_ROOT = Path(__file__).resolve().parent

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
    "structural_patterns": 0.15,   # Layer D4 : CAS-loop + import + identifier vocab
}

REFERENCES_DIR  = _PROJECT_ROOT / "references"
CATEGORIZED_DIR = _PROJECT_ROOT / "ground truth"

# Annotation JSON sidecars (redesigned schema)
_ANNOTATION_FILES: Dict[str, str] = {
    "linked_list":        "linked_list_annotation.json",
    "binary_search_tree": "binary_search_tree_annotation.json",
    "b_minus_tree":       "b_minus_tree_annotation.json",
    "queue":              "queue_annotation.json",
    "stack":              "stack_annotation.json",
}

# Maps ds_name → subfolder in categorized/
# None  → no pool → Layer C = 0.0
_CATEGORIZED_DS_MAP: Dict[str, Optional[str]] = {
    "linked_list":        "linked_list",
    "binary_search_tree": "bst",
    "b_minus_tree":       "bplus_tree",
    "skiplist":           "skip_list",
    "hash_table":         "hash_table",
    "queue":              "queue",
    "stack":              "stack",
}

# DS-specific identifier vocabularies for Layer D4c
_DS_IDENTIFIER_VOCAB: Dict[str, List[str]] = {
    "linked_list": [
        "head", "tail", "sentinel", "dummy", "next", "pred", "curr", "succ",
        "marked", "key", "val", "window", "find", "snip", "tryMark",
        "AtomicMarkableReference", "getReference", "isMarked", "compareAndSet",
    ],
    "binary_search_tree": [
        "root", "left", "right", "parent", "grandparent", "leaf", "internal",
        "key", "val", "Info", "IInfo", "DInfo", "Flag", "Mark", "Update",
        "find", "search", "insert", "delete", "compareAndSet",
        "AtomicReference", "AtomicStampedReference",
    ],
    "b_minus_tree": [
        "root", "keys", "children", "isLeaf", "leaf", "internal",
        "split", "merge", "redistribute", "order", "degree", "MAX_KEYS",
        "record", "LeafRecord", "compareAndSet", "AtomicReference",
        "copyWith", "snapshot",
    ],
    "quad_tree": [
        "root", "nw", "ne", "sw", "se", "children", "bounds", "region",
        "quadrant", "minX", "maxX", "minY", "maxY", "insert", "search",
        "compareAndSet", "AtomicReference",
    ],
    "skiplist": [
        "head", "sentinel", "next", "level", "height", "maxLevel",
        "forward", "marked", "fullyLinked", "pred", "succ", "find",
        "compareAndSet", "AtomicMarkableReference", "AtomicReference",
    ],
    "hash_table": [
        "table", "bucket", "hash", "hashCode", "slot", "chain",
        "resize", "rehash", "load", "capacity", "threshold",
        "head", "next", "key", "val", "compareAndSet", "AtomicReference",
        "splitOrder", "makeBucket", "reverseKey",
    ],
    "queue": [
        "head", "tail", "sentinel", "dummy", "next", "val",
        "enqueue", "dequeue", "isEmpty", "offer", "poll",
        "compareAndSet", "AtomicReference",
    ],
    "stack": [
        "top", "next", "val", "push", "pop", "peek", "isEmpty",
        "compareAndSet", "AtomicReference",
    ],
}

# Module-level caches
_categorized_cache: Dict[str, List[Tuple[str, str]]] = {}
_annotation_cache:  Dict[str, Optional[Dict]]         = {}


# =============================================================================
# ── Utility: loaders ──────────────────────────────────────────────────────────
# =============================================================================

def _load_annotation(ds_name: str) -> Optional[Dict]:
    """Load and cache the annotation JSON sidecar."""
    if ds_name in _annotation_cache:
        return _annotation_cache[ds_name]
    fname = _ANNOTATION_FILES.get(ds_name)
    if not fname:
        _annotation_cache[ds_name] = None
        return None
    path = REFERENCES_DIR / fname
    if not path.exists():
        _annotation_cache[ds_name] = None
        return None
    try:
        ann = json.loads(path.read_text(encoding="utf-8"))
        _annotation_cache[ds_name] = ann
        return ann
    except Exception:
        _annotation_cache[ds_name] = None
        return None


def _load_all_categorized(ds_name: str) -> List[Tuple[str, str]]:
    """Load all .java files from categorized/<folder>/ for a DS. Cached."""
    if ds_name in _categorized_cache:
        return _categorized_cache[ds_name]
    folder_name = _CATEGORIZED_DS_MAP.get(ds_name)
    if folder_name is None:
        _categorized_cache[ds_name] = []
        return []
    folder = CATEGORIZED_DIR / folder_name
    if not folder.exists():
        _categorized_cache[ds_name] = []
        return []
    results: List[Tuple[str, str]] = []
    for java_file in sorted(folder.glob("*.java")):
        try:
            code = java_file.read_text(encoding="utf-8", errors="ignore")
            results.append((java_file.name, code))
        except Exception:
            pass
    _categorized_cache[ds_name] = results
    return results


# =============================================================================
# Layer A — Consistency Test (multiplier: 1.0 pass / 0.0 fail)
# =============================================================================

def score_consistency_test(test_results: Optional[Dict]) -> Tuple[float, str]:
    """
    Layer A — hard binary multiplier.
    1.0 → sanity_status == 'pass'
    0.0 → anything else (logged but not weighted)
    """
    if not test_results:
        return 0.0, "test_results not provided"
    status = str(test_results.get("sanity_status", "none"))
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
    """
    if not test_results:
        return 0.0, "test_results not provided"
    semantic = str(test_results.get("lock_freedom_status", "none"))
    syntax   = str(test_results.get("lock_syntax_status",  "unknown"))
    if semantic == "lock-free":
        return 1.0, f"Semantic=lock-free ✓ (syntax={syntax}, diagnostic only)"
    return 0.0, f"Semantic={semantic} ✗ (syntax={syntax})"


# =============================================================================
# Layer C — Multi-Reference CodeBLEU (MAX over ground-truth pool)
# =============================================================================

def _bleu4_fallback(generated: str, reference: str) -> Tuple[float, str]:
    """Pure n-gram BLEU-4 — no external deps beyond stdlib."""
    try:
        def _tok(s: str) -> List[str]:
            return re.findall(r"[a-zA-Z_][a-zA-Z0-9_]*|[^\s\w]", s.lower())

        ref_toks = _tok(reference)
        hyp_toks = _tok(generated)
        if not ref_toks or not hyp_toks:
            return 0.0, "bleu_fallback_empty"

        precisions = []
        for n in range(1, 5):
            ref_ng: Dict[tuple, int] = {}
            for i in range(len(ref_toks) - n + 1):
                ng = tuple(ref_toks[i:i+n])
                ref_ng[ng] = ref_ng.get(ng, 0) + 1
            total = max(0, len(hyp_toks) - n + 1)
            if total == 0:
                precisions.append(0.0); continue
            hyp_ng: Dict[tuple, int] = {}
            for i in range(total):
                ng = tuple(hyp_toks[i:i+n])
                hyp_ng[ng] = hyp_ng.get(ng, 0) + 1
            match = sum(min(cnt, ref_ng.get(ng, 0)) for ng, cnt in hyp_ng.items())
            precisions.append(match / total)

        if all(p == 0.0 for p in precisions):
            return 0.0, "bleu_fallback_zero"
        bp   = min(1.0, math.exp(1 - len(ref_toks) / max(len(hyp_toks), 1)))
        lavg = sum(math.log(max(p, 1e-12)) for p in precisions) / 4.0
        return round(float(bp * math.exp(lavg)), 4), "bleu_fallback"
    except Exception as exc:
        return 0.0, f"error:{exc}"


def _compute_codebleu_safe(generated: str, reference: str) -> Tuple[float, str]:
    """
    Compute CodeBLEU with 3-level fallback. Never raises.
    Level 1: full calc_codebleu (ngram + weighted_ngram + AST + dataflow)
    Level 2: ngram + weighted_ngram only (skips tree-sitter)
    Level 3: pure BLEU-4 (stdlib only)
    """
    if not _CODEBLEU_AVAILABLE:
        return _bleu4_fallback(generated, reference)

    try:
        r = calc_codebleu([reference], [generated], lang="java",
                          weights=(0.25, 0.25, 0.25, 0.25))
        return float(r.get("codebleu", 0.0)), "full"
    except Exception:
        pass

    try:
        r = calc_codebleu([reference], [generated], lang="java",
                          weights=(0.5, 0.5, 0.0, 0.0))
        return float(r.get("codebleu", 0.0)), "ngram_only"
    except Exception:
        pass

    return _bleu4_fallback(generated, reference)


def score_multi_ref_codebleu(code: str, ds_name: str) -> Tuple[float, str]:
    """
    Layer C: Compare against every reference in categorized/<folder>/.
    Returns the MAXIMUM score (best-matching reference).
    Also reports avg/std/n in the detail string for transparency.

    If no pool exists → 0.0 with clear message.
    """
    if _CATEGORIZED_DS_MAP.get(ds_name) is None:
        return 0.0, f"No ground-truth pool for '{ds_name}' (not scoped — Layer C=0.0)"

    refs = _load_all_categorized(ds_name)
    if not refs:
        folder = _CATEGORIZED_DS_MAP.get(ds_name, "?")
        return 0.0, f"categorized/{folder}/ is empty or missing"

    per_ref_scores: List[float] = []
    per_ref_details: List[str]  = []

    for (fname, ref_code) in refs:
        if len(ref_code.strip()) < 100:
            per_ref_details.append(f"{fname}=SKIP(too_short)")
            continue
        score_i, method_i = _compute_codebleu_safe(code, ref_code)
        per_ref_scores.append(score_i)
        per_ref_details.append(f"{fname}={score_i:.4f}[{method_i}]")

    if not per_ref_scores:
        return 0.0, "all references skipped or errored"

    n      = len(per_ref_scores)
    best   = max(per_ref_scores)                      # ← MAX (not avg)
    avg    = sum(per_ref_scores) / n
    var    = sum((s - avg) ** 2 for s in per_ref_scores) / n
    std    = var ** 0.5

    detail = (
        f"max={best:.4f} avg={avg:.4f} std={std:.4f} n={n} | "
        + " | ".join(per_ref_details[:8])
    )
    return round(best, 4), detail


# =============================================================================
# Layer D1 — Annotation-Driven Criterion Match
# =============================================================================

def _check_invariants(code: str, invariants: Dict[str, bool]) -> int:
    """Return count of structural invariants that passed."""
    _POS = {
        "has_sentinel_head":                   r"\b(head|sentinel|dummy)\b",
        "has_sentinel_tail":                   r"\b(tail|sentinel)\b",
        "node_has_next_field":                 r"\bnext\b",
        "node_has_left_field":                 r"\bleft\b",
        "node_has_right_field":                r"\bright\b",
        "has_key_comparison_logic":            r"(key\s*[<>]=?|compareTo|\.key\s*[<>])",
        "has_downward_traversal":              r"while.*child|for.*child|instanceof.*Node",
        "has_window_or_find_method":           r"\b(find|window|Window|search)\b",
        "find_does_physical_cleanup":          r"compareAndSet|snip",
        "has_level_or_height_field":           r"\b(level|height|maxLevel|MAX_LEVEL)\b",
        "node_has_next_array_or_multi_level":  r"next\s*\[|AtomicMarkableReference",
        "uses_mark_bit_for_deletion":          r"AtomicMarkableReference|isMarked",
        "node_has_four_children":              r"\[\s*4\s*\]|children",
        "children_are_atomic_references":      r"AtomicReference",
        "has_spatial_bounds_or_coordinates":   r"\b(minX|maxX|minY|maxY|bounds|region|quadrant)\b",
        "has_quadrant_selection_logic":        r"quadrant|NW|NE|SW|SE|nw|ne|sw|se",
        "has_deleted_sentinel_or_mark":        r"DELETED|AtomicMarkableReference|isMarked",
        "has_isLeaf_or_leaf_flag":             r"\b(isLeaf|leaf|isInternal)\b",
        "node_has_keys_array_or_list":         r"\b(keys|entries)\b",
        "node_has_children_array_or_list":     r"\b(children|child)\b",
        "has_order_or_max_keys_constant":      r"\b(order|degree|MAX_KEYS|maxKeys)\b",
        "uses_immutable_record_or_snapshot":   r"(LeafRecord|Record|Snapshot|immutable)",
        "has_leaf_and_internal_distinction":   r"(Leaf|Internal|isLeaf|isInternal)",
        "find_does_physical_cleanup_at_each_level": r"compareAndSet",
        "contains_is_wait_free":               r"boolean\s+contains|contains\b",
    }
    _NEG = {
        "node_has_no_left_right_children": r"\b(left|right)\b",
        "node_has_no_next_field":          r"\bnext\b",
        "node_has_no_left_right_fields":   r"\b(left|right)\b",
    }
    passed = 0
    for key, expected in invariants.items():
        if key in _POS:
            try:
                found = bool(re.search(_POS[key], code, re.IGNORECASE | re.DOTALL))
            except re.error:
                found = _POS[key].split("|")[0].lower() in code.lower()
            if found == bool(expected):
                passed += 1
        elif key in _NEG:
            try:
                found = bool(re.search(_NEG[key], code, re.IGNORECASE | re.DOTALL))
            except re.error:
                found = _NEG[key].lower() in code.lower()
            if bool(expected) and not found:
                passed += 1
            elif not bool(expected) and found:
                passed += 1
        else:
            passed += 1   # unknown → benefit of the doubt
    return passed


def score_annotation_match(code: str, ds_name: str) -> Tuple[float, str]:
    """
    Layer D1: Score against annotation JSON sidecar.
    Checks: atomic class, required keywords, forbidden keywords, structural invariants.
    """
    ann = _load_annotation(ds_name)
    if not ann:
        return 0.5, f"no annotation for '{ds_name}' — neutral 0.5"

    w_cfg = ann.get("annotation_score_weights", {
        "atomic_class_match":        0.25,
        "required_keywords_match":   0.35,
        "forbidden_keywords_absent": 0.20,
        "structural_invariants":     0.20,
    })
    score   = 0.0
    details: List[str] = []

    # 1. Atomic class
    w  = w_cfg.get("atomic_class_match", 0.25)
    af = ann.get("atomic_fields", {})
    all_classes = [af.get("primary_class", "")] + af.get("alt_classes", [])
    found_cls = next((c for c in all_classes if c and c in code), None)
    if found_cls:
        score += w
        details.append(f"atomic class '{found_cls}' ✓")
    else:
        details.append(f"MISSING atomic class (need {af.get('primary_class','')})")

    # 2. Required keywords
    w       = w_cfg.get("required_keywords_match", 0.35)
    req_kws = ann.get("required_keywords", [])
    if req_kws:
        matched = []
        for kw in req_kws:
            try:
                if re.search(kw, code, re.IGNORECASE):
                    matched.append(kw.split("|")[0])
            except re.error:
                if kw.lower() in code.lower():
                    matched.append(kw)
        ratio = len(matched) / len(req_kws)
        score += w * ratio
        details.append(f"req kws {len(matched)}/{len(req_kws)} ✓" if matched
                       else f"MISSING all {len(req_kws)} required keywords")

    # 3. Forbidden keywords absent
    w        = w_cfg.get("forbidden_keywords_absent", 0.20)
    forb_kws = ann.get("forbidden_keywords", [])
    found_forb = [kw for kw in forb_kws
                  if re.search(re.escape(kw), code, re.IGNORECASE)]
    if not found_forb:
        score += w
        details.append("no forbidden keywords ✓")
    else:
        details.append(f"WARNING: forbidden: {found_forb[:3]}")

    # 4. Structural invariants
    w    = w_cfg.get("structural_invariants", 0.20)
    invs = ann.get("structural_invariants", {})
    if invs:
        passed = _check_invariants(code, invs)
        score += w * (passed / len(invs))
        details.append(f"invariants {passed}/{len(invs)} ✓")

    return round(min(score, 1.0), 4), " | ".join(details)


# =============================================================================
# Layer D2 — Concurrency Primitives
# =============================================================================

def score_concurrency(code: str) -> Tuple[float, str]:
    """Layer D2: CAS/Atomic patterns present, locking primitives absent."""
    details: List[str] = []
    score = 0.0

    atomic_patterns = [
        r"AtomicReference", r"AtomicMarkableReference", r"AtomicStampedReference",
        r"AtomicReferenceFieldUpdater", r"AtomicInteger", r"AtomicBoolean",
        r"compareAndSet", r"getAndSet", r"compareAndExchange", r"volatile",
    ]
    found_atomic = [p for p in atomic_patterns if re.search(p, code)]
    if found_atomic:
        score += min(0.6, 0.12 * len(found_atomic))
        details.append(f"CAS primitives: {len(found_atomic)}")
    else:
        details.append("MISSING: no CAS/Atomic primitives")

    if re.search(r"compareAndSet\s*\(", code):
        score += 0.2
        details.append("compareAndSet() call ✓")

    lock_patterns = [
        r"\bsynchronized\b", r"ReentrantLock", r"\.lock\(\)", r"\.unlock\(\)",
        r"StampedLock", r"ReadWriteLock", r"\bwait\s*\(", r"\bnotify\s*\(",
        r"Semaphore", r"CountDownLatch",
    ]
    found_locks = [p for p in lock_patterns if re.search(p, code)]
    if found_locks:
        score -= 0.4
        details.append(f"WARNING: lock primitives: {len(found_locks)}")
    else:
        score += 0.2
        details.append("no lock primitives ✓")

    return max(0.0, min(score, 1.0)), " | ".join(details)


# =============================================================================
# Layer D3 — LLM-as-Judge
# =============================================================================

_JUDGE_SYSTEM = (
    "You are a code auditor specializing in concurrent data structures. "
    "Given Java code, determine if it actually implements the specified data structure. "
    "Be strict: a Set ADT backed by a linked list is NOT a binary search tree. "
    "Judge by structural properties and algorithm, not the interface."
)

_JUDGE_USER_TEMPLATE = """\
Data structure claimed: {ds_name}

Java code to evaluate:
{code}

Respond in EXACTLY this format (no extra text):
VERDICT: YES
REASON: <one sentence>

Or:
VERDICT: NO
REASON: <one sentence explaining what it actually is>
"""


def score_llm_judge(
    code: str, ds_name: str, timeout: int = 45, max_retries: int = 3,
) -> Tuple[float, str, str]:
    """Layer D3: LLM-as-judge. YES→1.0, NO→0.0, SKIP/ERROR→0.5."""
    import time

    if not _LLM_AVAILABLE:
        return 0.5, "SKIP", "langchain-openai not installed"
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    if not api_key:
        return 0.5, "SKIP", "NVIDIA_NIM_API_KEY not set"

    model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
    from langchain_core.messages import SystemMessage, HumanMessage
    messages = [
        SystemMessage(content=_JUDGE_SYSTEM),
        HumanMessage(content=_JUDGE_USER_TEMPLATE.format(
            ds_name=ds_name, code=code[:6000])),
    ]
    last_exc: Optional[Exception] = None
    for attempt in range(1, max_retries + 1):
        try:
            llm = ChatOpenAI(
                base_url="https://integrate.api.nvidia.com/v1",
                api_key=api_key, model=model,
                temperature=0.0, max_tokens=200, request_timeout=timeout,
            )
            response = llm.invoke(messages)
            text = response.content if hasattr(response, "content") else str(response)
            vm = re.search(r"VERDICT:\s*(YES|NO)", text, re.IGNORECASE)
            rm = re.search(r"REASON:\s*(.+)", text, re.DOTALL)
            verdict = vm.group(1).upper() if vm else "NO"
            reason  = rm.group(1).strip()[:200] if rm else text[:200]
            return (1.0 if verdict == "YES" else 0.0), verdict, reason
        except Exception as exc:
            last_exc = exc
            exc_str  = str(exc)
            is_rl = "429" in exc_str or "rate" in exc_str.lower() or "temporarily" in exc_str.lower()
            if is_rl and attempt < max_retries:
                wait = 2 ** attempt
                print(f"  [LLM Judge] Rate limit (attempt {attempt}). Retry in {wait}s…")
                time.sleep(wait)
                continue
            break
    return 0.5, "ERROR", f"LLM judge failed after {max_retries} attempts: {last_exc}"


# =============================================================================
# Layer D4 — Structural Patterns Sub-Bundle
# =============================================================================

def score_structural_patterns(code: str, ds_name: str) -> Tuple[float, str]:
    """
    Layer D4: Three sub-checks, weighted internally.
      D4a (0.45): CAS-loop retry pattern — while(true) { ... compareAndSet ... }
      D4b (0.25): Import correctness    — java.util.concurrent.atomic imported
      D4c (0.30): Identifier semantics  — token overlap with DS vocabulary
    """
    details: List[str] = []
    score = 0.0

    # ── D4a: CAS-loop retry pattern (weight 0.45) ──────────────────────────────
    W_a = 0.45
    # Pattern 1: while(true) or do-while loop containing compareAndSet
    has_while_cas = bool(re.search(
        r"while\s*\(\s*(true|!?\w+\.?\w*\s*\(?\)?)\s*\)[\s\S]{0,500}?compareAndSet",
        code, re.IGNORECASE | re.DOTALL
    ))
    # Pattern 2: for(;;) with compareAndSet
    has_for_cas = bool(re.search(
        r"for\s*\(\s*;\s*;\s*\)[\s\S]{0,500}?compareAndSet",
        code, re.IGNORECASE | re.DOTALL
    ))
    # Pattern 3: do-while retry
    has_do_cas = bool(re.search(
        r"do\s*\{[\s\S]{0,500}?compareAndSet[\s\S]{0,200}?\}\s*while",
        code, re.IGNORECASE | re.DOTALL
    ))
    # Pattern 4: labelled retry (retry: label + continue)
    has_label_retry = bool(re.search(
        r"(retry|loop|again)\s*:\s*[\s\S]{0,300}?compareAndSet",
        code, re.IGNORECASE | re.DOTALL
    ))
    cas_loop = has_while_cas or has_for_cas or has_do_cas or has_label_retry
    if has_while_cas:
        score += W_a; details.append("CAS-loop while(true)+CAS ✓")
    elif has_for_cas:
        score += W_a * 0.9; details.append("CAS-loop for(;;)+CAS ✓")
    elif has_do_cas:
        score += W_a * 0.8; details.append("CAS-loop do-while+CAS ✓")
    elif has_label_retry:
        score += W_a * 0.7; details.append("CAS-loop labelled-retry+CAS ✓")
    else:
        details.append("MISSING: no CAS-loop retry pattern")

    # ── D4b: Import correctness (weight 0.25) ──────────────────────────────────
    W_b = 0.25
    has_atomic_import = bool(re.search(
        r"import\s+java\.util\.concurrent\.atomic", code, re.IGNORECASE
    ))
    has_lock_import = bool(re.search(
        r"import\s+java\.util\.concurrent\.locks", code, re.IGNORECASE
    ))
    if has_atomic_import and not has_lock_import:
        score += W_b; details.append("import concurrent.atomic ✓")
    elif has_atomic_import and has_lock_import:
        score += W_b * 0.5; details.append("import atomic✓ BUT also locks (partial)")
    else:
        details.append("MISSING: no java.util.concurrent.atomic import")

    # ── D4c: Identifier semantic overlap with DS vocabulary (weight 0.30) ──────
    W_c   = 0.30
    vocab = _DS_IDENTIFIER_VOCAB.get(ds_name, [])
    if vocab:
        # Tokenize code — extract all identifiers
        code_identifiers = set(re.findall(r"\b[A-Za-z_][A-Za-z0-9_]*\b", code))
        vocab_set        = set(vocab)
        hit_count        = len(code_identifiers & vocab_set)
        ratio            = hit_count / len(vocab_set)
        score += W_c * min(ratio * 2.5, 1.0)   # scale: 40% vocab hit → full credit
        details.append(
            f"vocab overlap {hit_count}/{len(vocab_set)} "
            f"({ratio*100:.0f}%) ✓" if hit_count > 0
            else "MISSING: no vocabulary overlap"
        )
    else:
        score += W_c * 0.5   # neutral for unknown DS
        details.append(f"no vocab defined for '{ds_name}' — neutral")

    return round(min(score, 1.0), 4), " | ".join(details)


# =============================================================================
# ── Combined scorer ───────────────────────────────────────────────────────────
# =============================================================================

def evaluate_file(
    java_file: Path,
    ds_name: str,
    weights: Dict[str, float] = DEFAULT_WEIGHTS,
    use_llm_judge: bool = True,
    approach: str = "unknown",
    test_results: Optional[Dict[str, str]] = None,
) -> Dict[str, Any]:
    """
    Evaluate a Java file against the 4-layer benchmark.

    Layers A & B are MULTIPLIERS:
      - Still reported and logged (pass/fail) in results
      - If either is 0.0 → combined_score is forced to 0.0
      - They carry no weight in the weighted sum

    Layers C + D1 + D2 + D3 + D4 are WEIGHTED (must sum to 1.0).

    Parameters
    ----------
    java_file    : path to generated ConcurrentDataStructure.java
    ds_name      : 'linked_list' | 'binary_search_tree' | 'b_minus_tree' | etc.
    weights      : override DEFAULT_WEIGHTS (C + D layers only)
    use_llm_judge: run Layer D3 LLM-as-judge
    approach     : 'zero_shot' or 'translation' (for logging)
    test_results : dict with sanity_status, lock_freedom_status, lock_syntax_status
    """
    result: Dict[str, Any] = {
        "file":      str(java_file),
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
        "annotation_score":          0.5,
        "annotation_detail":         "",
        "concurrency_score":         0.0,
        "concurrency_detail":        "",
        "llm_judge_score":           0.5,
        "llm_judge_verdict":         "SKIP",
        "llm_judge_reason":          "",
        "structural_patterns_score": 0.0,
        "structural_patterns_d4a":   "",
        "structural_patterns_detail":"",
        # Combined
        "combined_score":  0.0,
        "is_correct_ds":   False,
        "error":           "",
    }

    # Guard
    if not java_file.exists():
        result["error"] = f"File not found: {java_file}"
        return result
    try:
        code = java_file.read_text(encoding="utf-8", errors="ignore")
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

    # ── Layer C: Multi-Reference CodeBLEU (max) ────────────────────────────────
    c_score, c_detail = score_multi_ref_codebleu(code, ds_name)
    result["multi_ref_cb_score"]  = round(c_score, 4)
    result["multi_ref_cb_detail"] = c_detail

    # ── Layer D1: Annotation Match ─────────────────────────────────────────────
    d1_score, d1_detail = score_annotation_match(code, ds_name)
    result["annotation_score"]  = round(d1_score, 4)
    result["annotation_detail"] = d1_detail

    # ── Layer D2: Concurrency Primitives ───────────────────────────────────────
    d2_score, d2_detail = score_concurrency(code)
    result["concurrency_score"]  = round(d2_score, 4)
    result["concurrency_detail"] = d2_detail

    # ── Layer D3: LLM Judge ────────────────────────────────────────────────────
    if use_llm_judge:
        d3_score, d3_verdict, d3_reason = score_llm_judge(code, ds_name)
        result["llm_judge_score"]   = round(d3_score, 4)
        result["llm_judge_verdict"] = d3_verdict
        result["llm_judge_reason"]  = d3_reason

    # ── Layer D4: Structural Patterns ──────────────────────────────────────────
    d4_score, d4_detail = score_structural_patterns(code, ds_name)
    result["structural_patterns_score"]  = round(d4_score, 4)
    result["structural_patterns_detail"] = d4_detail

    # ── Weighted combination (C + D layers) ────────────────────────────────────
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
    "linked_list":        ["linked_list", "linkedlist", "ll"],
    "binary_search_tree": ["binary_search_tree", "bst", "binarysearchtree"],
    "quad_tree":          ["quad_tree", "quadtree", "qt"],
    "skiplist":           ["skiplist", "skip_list"],
    "b_minus_tree":       ["b_minus_tree", "b_tree", "btree", "bminus"],
}


def _guess_ds_from_path(path: Path) -> str:
    path_str = str(path).lower().replace("\\", "/")
    for ds, aliases in _DS_ALIASES.items():
        if any(a in path_str for a in aliases):
            return ds
    return "unknown"


def _guess_approach_from_path(path: Path) -> str:
    path_str = str(path).lower()
    return "zero_shot" if ("zero_shot" in path_str or "zeroshot" in path_str) else "translation"


def _find_java_files(root: Path, ds_name: Optional[str] = None) -> List[Path]:
    results = []
    for p in root.rglob("ConcurrentDataStructure.java"):
        results.append(p)
    for p in root.rglob("*.java"):
        if "Concurrent" in p.name and p not in results:
            results.append(p)
    return results


def _infer_additional_context(java_path: Path) -> Dict[str, str]:
    parts = java_path.parts
    ctx   = {"model": "unknown", "prompt_idx": "?", "sample_idx": "?"}
    for i, part in enumerate(parts):
        if re.match(r"prompt_\d+_sample_\d+", part):
            m = re.match(r"prompt_(\d+)_sample_(\d+)", part)
            if m:
                ctx["prompt_idx"] = m.group(1)
                ctx["sample_idx"] = m.group(2)
        elif part in ("generated_code_zero_shot", "generated_code"):
            if i + 1 < len(parts):
                ctx["model"] = parts[i + 1]
    return ctx


CSV_HEADERS_BATCH = [
    "file", "ds_name", "approach", "model", "prompt_idx", "sample_idx",
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
    def _c(v):
        return str(v).replace(",", ";").replace("\n", " ").replace("\r", "")
    return [
        _c(r.get("file", "")), _c(r.get("ds_name", "")), _c(r.get("approach", "")),
        _c(ctx.get("model", "unknown")), _c(ctx.get("prompt_idx", "?")), _c(ctx.get("sample_idx", "?")),
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
    total   = len(results)
    correct = sum(1 for r in results if r.get("is_correct_ds"))
    avg_cb  = (sum(r.get("combined_score", 0) for r in results) / total) if total else 0
    print("\n" + "=" * 80)
    print(f"EXTENDED CODEBLEU SUMMARY  |  {total} files evaluated")
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
    _load_env()

    parser = argparse.ArgumentParser(
        description="Extended CodeBLEU — Semantic Correctness Checker for Lock-Free DS",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    mode_group = parser.add_mutually_exclusive_group(required=True)
    mode_group.add_argument("--file", type=Path)
    mode_group.add_argument("--dir",  type=Path)
    parser.add_argument("--dir2", type=Path, default=None)
    parser.add_argument("--ds", default=None, choices=list(_DS_ALIASES.keys()))
    parser.add_argument("--output", type=Path, default=None)
    parser.add_argument("--json-output", type=Path, default=None)
    parser.add_argument("--no-llm-judge", action="store_true")
    parser.add_argument("--model", default=None)
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
        for scan_dir in ([args.dir] + ([args.dir2] if args.dir2 else [])):
            if not scan_dir.exists():
                print(f"[WARN] Directory not found: {scan_dir}")
                continue
            for jf in _find_java_files(scan_dir, args.ds):
                ds       = args.ds or _guess_ds_from_path(jf)
                approach = _guess_approach_from_path(jf)
                files_to_eval.append((jf, ds, approach))

    if not files_to_eval:
        print("[ERROR] No files to evaluate.")
        sys.exit(1)

    print(f"\n[Extended CodeBLEU] Evaluating {len(files_to_eval)} file(s)...")
    print(f"  LLM judge : {'ON' if use_llm else 'OFF'}")
    print(f"  CodeBLEU  : {'ON' if _CODEBLEU_AVAILABLE else 'OFF (BLEU-4 fallback)'}\n")

    all_results: List[Dict[str, Any]] = []
    iterator = files_to_eval
    if _TQDM_AVAILABLE:
        iterator = tqdm(files_to_eval, desc="Evaluating")  # type: ignore

    for java_path, ds_name, approach in iterator:
        try:
            result = evaluate_file(java_path, ds_name, use_llm_judge=use_llm, approach=approach)
        except Exception as exc:
            result = {
                "file": str(java_path), "ds_name": ds_name, "approach": approach,
                "combined_score": 0.0, "is_correct_ds": False,
                "error": f"Exception: {exc}\n{traceback.format_exc()[:300]}",
            }
        all_results.append(result)

    _print_summary(all_results)

    output_csv = args.output or Path(f"semantic_results_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")
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
