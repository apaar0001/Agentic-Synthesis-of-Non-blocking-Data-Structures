"""
cpp_react/prepare_reprompt.py — Reprompt Nodes for C++ ReAct Pipeline
=======================================================================

Mirrors nodes/prepare_reprompt_sequential.py, prepare_reprompt_concurrent.py,
and prepare_reprompt_structural.py for Java.
"""
from __future__ import annotations

from typing import Dict, Any
from datetime import datetime

from .state import CppReactState


# ── Sequential reprompt ───────────────────────────────────────────────────────

def node_prepare_reprompt_seq(state: CppReactState) -> Dict[str, Any]:
    """Prepare reprompt for sequential phase failures (compilation errors)."""
    error = state.get("error_message", "Unknown error")
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    reprompt = (
        f"The previous C++ code failed to compile. Here is the error:\n\n"
        f"{error}\n\n"
        f"Please fix the code and provide the complete corrected C++ implementation. "
        f"Output ONLY the C++ code, no explanations."
    )

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [REPROMPT SEQ] ---\n")
        f.write(f"Error: {error[:200]}\n")
        f.flush()

    return {
        "current_prompt": reprompt,
    }


# ── Structural reprompt ───────────────────────────────────────────────────────

def node_prepare_reprompt_structural(state: CppReactState) -> Dict[str, Any]:
    """Prepare reprompt for structural verification failures."""
    expected = state.get("structural_expected", [])
    detected = state.get("structural_detected", [])
    missing = [e for e in expected if e not in detected]
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    issues = []
    if "std::atomic" in missing:
        issues.append("- Use std::atomic<Node*> for all shared pointers")
    if "compare_exchange" in missing:
        issues.append("- Use compare_exchange_strong() or compare_exchange_weak() for CAS operations")
    if "no_mutex" in missing:
        issues.append("- Remove ALL locks: std::mutex, std::lock_guard, std::unique_lock")
    if "class_name" in missing:
        issues.append("- The class must be named exactly 'ConcurrentDataStructure'")
    if "SetADT" in missing:
        issues.append("- The class must inherit from SetADT: class ConcurrentDataStructure : public SetADT")
    if "interface_methods" in missing:
        issues.append("- Must implement: bool contains(int key), bool add(int key), bool remove(int key)")

    issues_str = "\n".join(issues) if issues else "Structural checks failed"

    reprompt = (
        f"The concurrent C++ code has structural issues:\n\n"
        f"{issues_str}\n\n"
        f"Please fix these issues and provide the complete corrected lock-free "
        f"C++17 implementation. Output ONLY the C++ code."
    )

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [REPROMPT STRUCTURAL] ---\n")
        f.write(f"Missing: {missing}\n")
        f.flush()

    return {
        "current_prompt": reprompt,
        "structural_retry_used": True,
    }


# ── Concurrent reprompt ───────────────────────────────────────────────────────

def node_prepare_reprompt_con(state: CppReactState) -> Dict[str, Any]:
    """Prepare reprompt for concurrent phase failures (compile/test errors)."""
    error = state.get("error_message", "Unknown error")
    stage = state.get("failure_stage", "none")
    comp = state.get("compilation_status", "none")
    sanity = state.get("consistency_status", "none")
    lockf = state.get("lock_freedom_status", "none")
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    if stage == "compile" or comp == "fail":
        reprompt = (
            f"The concurrent C++ code failed to compile:\n\n"
            f"{error}\n\n"
            f"Please fix the compilation errors and provide the complete corrected "
            f"lock-free C++17 implementation. Output ONLY the C++ code."
        )
        retry_field = "compile_retry_used"
    elif stage == "first_sanity" or sanity == "fail":
        reprompt = (
            f"The concurrent C++ code compiled but failed the consistency test:\n\n"
            f"{error}\n\n"
            f"The consistency test runs 4 threads performing concurrent add/remove/contains "
            f"operations. The test failed, indicating a race condition or incorrect "
            f"concurrent behavior.\n\n"
            f"Common issues:\n"
            f"- Incorrect CAS loop logic\n"
            f"- Missing retry on CAS failure\n"
            f"- Incorrect memory ordering\n"
            f"- Not skipping logically deleted nodes during traversal\n\n"
            f"Please fix and provide the complete corrected code. Output ONLY C++ code."
        )
        retry_field = "first_sanity_retry_used"
    elif stage == "second_sanity" or lockf != "lock-free":
        reprompt = (
            f"The concurrent C++ code passed consistency but is NOT lock-free:\n\n"
            f"Lock freedom status: {lockf}\n"
            f"Lock syntax status: {state.get('lock_syntax_status', 'unknown')}\n\n"
            f"The code must be truly lock-free — no mutexes, no blocking primitives.\n"
            f"All synchronization must use std::atomic and compare_exchange.\n"
            f"Ensure the '// Node has been marked' comment is placed immediately after "
            f"the successful CAS that logically marks a node for deletion.\n\n"
            f"Please fix and provide the complete corrected code. Output ONLY C++ code."
        )
        retry_field = "second_sanity_retry_used"
    else:
        reprompt = (
            f"The concurrent C++ code has issues:\n\n"
            f"{error}\n\n"
            f"Please fix and provide the complete corrected code. Output ONLY C++ code."
        )
        retry_field = "first_sanity_retry_used"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [REPROMPT CON] ---\n")
        f.write(f"Stage: {stage} | Error: {error[:200]}\n")
        f.flush()

    return {
        "current_prompt": reprompt,
        retry_field: True,
    }


# ── Phase switch ──────────────────────────────────────────────────────────────

def node_switch_to_concurrent(state: CppReactState) -> Dict[str, Any]:
    """Switch from sequential to concurrent phase."""
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n{'='*60}\n")
        f.write(f"  SWITCHING TO CONCURRENT PHASE\n")
        f.write(f"  Sequential code preserved in conversation history\n")
        f.write(f"{'='*60}\n\n")
        f.flush()

    return {
        "phase": "conc",
    }


# ── Logging nodes ─────────────────────────────────────────────────────────────

def node_log_success(state: CppReactState) -> Dict[str, Any]:
    """Log successful pipeline completion."""
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n{'='*60}\n")
        f.write(f"  PIPELINE SUCCESS ✓\n")
        f.write(f"  DS: {state.get('data_structure')}\n")
        f.write(f"  Seq attempts: {state.get('seq_attempt_count', 0)}\n")
        f.write(f"  Conc attempts: {state.get('conc_attempt_count', 0)}\n")
        f.write(f"{'='*60}\n\n")
        f.flush()

    return {
        "test_result": "pass",
        "final_code": state.get("generated_code", ""),
    }


def node_log_failure(state: CppReactState) -> Dict[str, Any]:
    """Log pipeline failure."""
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n{'='*60}\n")
        f.write(f"  PIPELINE FAILURE ✗\n")
        f.write(f"  DS: {state.get('data_structure')}\n")
        f.write(f"  Seq attempts: {state.get('seq_attempt_count', 0)}\n")
        f.write(f"  Conc attempts: {state.get('conc_attempt_count', 0)}\n")
        f.write(f"  Error: {state.get('error_message', 'N/A')[:200]}\n")
        f.write(f"{'='*60}\n\n")
        f.flush()

    return {
        "test_result": "fail",
        "final_code": state.get("generated_code", ""),
    }
