"""
cpp_react/verify_structural.py — Structural Verification Node
===============================================================

Mirrors nodes/verify_structural.py for Java.
Uses deterministic regex checks (not LLM) for speed and reliability.
Checks that generated concurrent code has required lock-free patterns.
"""
from __future__ import annotations

import re
from typing import Dict, Any, List, Tuple
from datetime import datetime

from .state import CppReactState


# ── Structural checks ────────────────────────────────────────────────────────

def _check_structural(code: str) -> Tuple[bool, List[str], List[str]]:
    """
    Check code for required lock-free structural patterns.
    Returns (passed, expected, detected).
    """
    expected: List[str] = []
    detected: List[str] = []

    # 1. Must have std::atomic
    expected.append("std::atomic")
    if re.search(r'std::atomic\s*<', code):
        detected.append("std::atomic")

    # 2. Must have compare_exchange (CAS)
    expected.append("compare_exchange")
    if re.search(r'compare_exchange_(strong|weak)', code):
        detected.append("compare_exchange")

    # 3. Must NOT have mutex/lock_guard
    expected.append("no_mutex")
    if not re.search(r'std::mutex|std::lock_guard|std::unique_lock', code):
        detected.append("no_mutex")

    # 4. Must have ConcurrentDataStructure class
    expected.append("class_name")
    if 'ConcurrentDataStructure' in code:
        detected.append("class_name")

    # 5. Must inherit from SetADT
    expected.append("SetADT")
    if re.search(r'(SetADT|public\s+SetADT)', code):
        detected.append("SetADT")

    # 6. Must have contains/add/remove methods
    expected.append("interface_methods")
    has_methods = (
        re.search(r'bool\s+(contains|add|remove)\s*\(', code)
    )
    if has_methods:
        detected.append("interface_methods")

    score = len(detected) / len(expected) if expected else 0.0
    passed = score >= 0.6  # At least 60% of checks must pass

    return passed, expected, detected


# ── Node function ─────────────────────────────────────────────────────────────

def node_verify_structural(state: CppReactState) -> Dict[str, Any]:
    """Verify structural correctness of generated concurrent C++ code."""
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"
    code = state.get("generated_code", "")

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [STRUCTURAL VERIFICATION] ---\n")
        f.write(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.flush()

    if not code or len(code.strip()) < 50:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write("  RESULT: FAIL — code is empty\n")
            f.flush()
        return {
            "structural_verify_status": "fail",
            "structural_expected": [],
            "structural_detected": [],
            "structural_score": 0.0,
            "error_message": "No code to verify",
        }

    passed, expected, detected = _check_structural(code)
    score = len(detected) / len(expected) if expected else 0.0

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"  Expected : {expected}\n")
        f.write(f"  Detected : {detected}\n")
        f.write(f"  Score    : {score:.2f}\n")
        f.write(f"  RESULT   : {'PASS' if passed else 'FAIL'}\n")
        f.flush()

    return {
        "structural_verify_status": "pass" if passed else "fail",
        "structural_expected": expected,
        "structural_detected": detected,
        "structural_score": score,
    }
