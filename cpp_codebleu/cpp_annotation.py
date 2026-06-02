"""
cpp_annotation.py — C++ Annotation Scorer (mirrors Java extended_codebleu.py logic)
======================================================================================

Scoring model (matches Java score_annotation_match):
  1. Atomic/primitive class match          (weight 0.25)
  2. Required keywords match              (weight 0.35)
  3. Forbidden keywords absent            (weight 0.20)
  4. Structural invariants                (weight 0.20)

Annotation JSON schema (flat, mirrors Java):
  {
    "data_structure": "...",
    "language": "cpp",
    "atomic_fields": {
        "primary_class": "std::atomic",
        "alt_classes": ["atomic_load", "compare_exchange_strong", ...]
    },
    "required_keywords": [...],        # regex strings
    "forbidden_keywords": [...],       # plain strings
    "structural_invariants": { key: bool, ... },
    "annotation_score_weights": {      # optional override
        "atomic_class_match":        0.25,
        "required_keywords_match":   0.35,
        "forbidden_keywords_absent": 0.20,
        "structural_invariants":     0.20
    }
  }

Requirements: 4.6.4, 4.6.5
"""

import json
import re
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any


# =============================================================================
# Structural invariant patterns (mirrors Java _POS / _NEG in extended_codebleu)
# =============================================================================

# Positive invariants: re pattern that must (or must-not) exist
_POS: Dict[str, str] = {
    # Generic
    "has_sentinel_head":              r"\b(head|sentinel|dummy)\b",
    "has_sentinel_tail":              r"\b(tail|sentinel)\b",
    "node_has_next_field":            r"\bnext\b",
    "node_has_left_field":            r"\bleft\b",
    "node_has_right_field":           r"\bright\b",
    "has_key_comparison_logic":       r"(key\s*[<>]=?|<\s*key\b|key\s*<)",
    "has_downward_traversal":         r"while.*child|for.*child|left\b|right\b",
    "has_window_or_find_method":      r"\b(find|window|search|locate)\b",
    "find_does_physical_cleanup":     r"compare_exchange_(strong|weak)",
    "has_level_or_height_field":      r"\b(level|height|maxLevel|MAX_LEVEL|NUM_LEVELS)\b",
    "node_has_next_array_or_multi":   r"next\s*\[|forward\s*\[",
    "uses_mark_bit_for_deletion":     r"(uintptr_t|reinterpret_cast|is_marked|marked)",
    "node_has_four_children":         r"\[\s*4\s*\]|children",
    "children_are_atomic":            r"std::atomic\s*<",
    "has_spatial_bounds":             r"\b(minX|maxX|minY|maxY|bounds|region|quadrant)\b",
    "has_quadrant_selection_logic":   r"quadrant|NW|NE|SW|SE|nw|ne|sw|se",
    "has_deleted_sentinel_or_mark":   r"(DELETED|is_marked|marked|uintptr_t)",
    "has_isLeaf_or_leaf_flag":        r"\b(isLeaf|is_leaf|leaf|isInternal)\b",
    "node_has_keys_array":            r"\b(keys|entries)\b",
    "node_has_children_array":        r"\b(children|child)\b",
    "has_order_or_max_keys":          r"\b(order|degree|MAX_KEYS|maxKeys|MAX_ORDER)\b",
    "uses_immutable_record":          r"(const|immutable|snapshot|Record)",
    "has_leaf_and_internal":          r"(leaf|internal|is_leaf|isLeaf|isInternal)",
    "find_does_cleanup_each_level":   r"compare_exchange_(strong|weak)",
    "contains_is_wait_free":          r"bool\s+contains|contains\s*\(",
    "has_hash_function":              r"\b(hash|hashCode|hash_fn|bucket_index)\b",
    "has_bucket_array":               r"\b(bucket|table|slots|buckets)\b",
    "has_cas_operation":              r"compare_exchange_(strong|weak)",
    "has_atomic_pointer":             r"std::atomic\s*<",
    "has_memory_ordering":            r"std::memory_order|memory_order_(relaxed|acquire|release|acq_rel|seq_cst)",
    "has_hazard_ptr_or_rcu":          r"(hazard|rcu|epoch|retire|reclaim)",
}

_NEG: Dict[str, str] = {
    "node_has_no_left_right_children": r"\b(left|right)\b",
    "node_has_no_next_field":          r"\bnext\b",
    "node_has_no_left_right_fields":   r"\b(left|right)\b",
}


def _check_invariants(code: str, invariants: Dict[str, bool]) -> int:
    """Return count of structural invariants that passed (mirrors Java logic exactly)."""
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
            passed += 1   # unknown key → benefit of the doubt
    return passed


# =============================================================================
# Module-level cache
# =============================================================================

_annotation_cache: Dict[str, Optional[Dict]] = {}

# Maps ds_name → annotation JSON filename
_ANNOTATION_FILES: Dict[str, str] = {
    "linked_list":  "cpp_linked_list_annotation.json",
    "skiplist":     "cpp_skiplist_annotation.json",
    "bst":          "cpp_bst_annotation.json",
    "hash_table":   "cpp_hash_table_annotation.json",
}


def _load_annotation(ds_name: str, annotation_dir: Optional[Path] = None) -> Optional[Dict]:
    """Load and cache the annotation JSON for a given DS name."""
    cache_key = f"{ds_name}:{annotation_dir}"
    if cache_key in _annotation_cache:
        return _annotation_cache[cache_key]

    if annotation_dir is None:
        annotation_dir = Path(__file__).parent.parent / "references"

    fname = _ANNOTATION_FILES.get(ds_name)
    if not fname:
        _annotation_cache[cache_key] = None
        return None

    path = annotation_dir / fname
    if not path.exists():
        _annotation_cache[cache_key] = None
        return None

    try:
        ann = json.loads(path.read_text(encoding="utf-8"))
        _annotation_cache[cache_key] = ann
        return ann
    except Exception:
        _annotation_cache[cache_key] = None
        return None


# =============================================================================
# Main scorer — mirrors Java score_annotation_match exactly
# =============================================================================

def score_cpp_annotation(code: str, ds_name: str,
                          annotation_dir: Optional[Path] = None) -> Tuple[float, Dict[str, Any]]:
    """
    Score C++ code against the annotation JSON sidecar.

    Mirrors Java score_annotation_match:
      Sub-score 1: atomic_class_match        (weight 0.25)
      Sub-score 2: required_keywords_match   (weight 0.35)
      Sub-score 3: forbidden_keywords_absent (weight 0.20)
      Sub-score 4: structural_invariants     (weight 0.20)

    Returns:
        (score, details_dict) where score ∈ [0.0, 1.0]
    """
    ann = _load_annotation(ds_name, annotation_dir)
    if not ann:
        return 0.5, {
            "score": 0.5,
            "required_passed": 0,
            "required_total": 0,
            "achieved_weight": 0.5,
            "total_weight": 1.0,
            "detail": f"no annotation for '{ds_name}' — neutral 0.5",
        }

    # Weight config (can be overridden per-annotation)
    w_cfg = ann.get("annotation_score_weights", {
        "atomic_class_match":        0.25,
        "required_keywords_match":   0.35,
        "forbidden_keywords_absent": 0.20,
        "structural_invariants":     0.20,
    })

    score = 0.0
    details: List[str] = []
    required_passed = 0
    required_total = 0

    # ── 1. Atomic/primitive class ────────────────────────────────────────────
    w = w_cfg.get("atomic_class_match", 0.25)
    af = ann.get("atomic_fields", {})
    all_classes = [af.get("primary_class", "")] + af.get("alt_classes", [])
    found_cls = next((c for c in all_classes if c and c in code), None)
    if found_cls:
        score += w
        details.append(f"atomic class '{found_cls}' ✓")
    else:
        details.append(f"MISSING atomic class (need {af.get('primary_class', '')})")

    # ── 2. Required keywords ─────────────────────────────────────────────────
    w = w_cfg.get("required_keywords_match", 0.35)
    req_kws = ann.get("required_keywords", [])
    required_total = len(req_kws)
    if req_kws:
        matched = []
        for kw in req_kws:
            try:
                if re.search(kw, code, re.IGNORECASE):
                    matched.append(kw.split("|")[0])
            except re.error:
                if kw.lower() in code.lower():
                    matched.append(kw)
        required_passed = len(matched)
        ratio = required_passed / len(req_kws)
        score += w * ratio
        details.append(
            f"req kws {required_passed}/{len(req_kws)} ✓" if matched
            else f"MISSING all {len(req_kws)} required keywords"
        )

    # ── 3. Forbidden keywords absent ─────────────────────────────────────────
    w = w_cfg.get("forbidden_keywords_absent", 0.20)
    forb_kws = ann.get("forbidden_keywords", [])
    found_forb = [kw for kw in forb_kws
                  if re.search(re.escape(kw), code, re.IGNORECASE)]
    if not found_forb:
        score += w
        details.append("no forbidden keywords ✓")
    else:
        details.append(f"WARNING: forbidden: {found_forb[:3]}")

    # ── 4. Structural invariants ─────────────────────────────────────────────
    w = w_cfg.get("structural_invariants", 0.20)
    invs = ann.get("structural_invariants", {})
    if invs:
        passed = _check_invariants(code, invs)
        score += w * (passed / len(invs))
        details.append(f"invariants {passed}/{len(invs)} ✓")

    final_score = round(min(score, 1.0), 4)

    return final_score, {
        "score": final_score,
        "required_passed": required_passed,
        "required_total": required_total,
        "achieved_weight": score,
        "total_weight": 1.0,
        "detail": " | ".join(details),
    }


# ── Backwards-compatible alias used by cpp_extended_codebleu.py ──────────────

def score_cpp_file(code: str, ds_name: str,
                   annotation_dir: Optional[Path] = None) -> Tuple[float, Dict[str, Any]]:
    """Alias kept for backward compatibility with cpp_extended_codebleu.py."""
    return score_cpp_annotation(code, ds_name, annotation_dir)


# =============================================================================
# CLI helper
# =============================================================================

def main():
    import sys
    if len(sys.argv) < 3:
        print("Usage: python cpp_annotation.py <cpp_file> <ds_name>")
        print("  ds_name: linked_list | skiplist | bst | hash_table")
        sys.exit(1)

    cpp_file = Path(sys.argv[1])
    ds_name  = sys.argv[2]

    if not cpp_file.exists():
        print(f"Error: File not found: {cpp_file}")
        sys.exit(1)

    code = cpp_file.read_text(encoding="utf-8", errors="ignore")
    score, details = score_cpp_file(code, ds_name)

    print(f"\n{'='*60}")
    print(f"Annotation Score for {cpp_file.name} ({ds_name})")
    print(f"{'='*60}")
    print(f"Overall Score : {score:.4f}")
    print(f"Detail        : {details['detail']}")
    print(f"Req kws       : {details['required_passed']} / {details['required_total']}")
    print(f"{'='*60}\n")


if __name__ == "__main__":
    main()
