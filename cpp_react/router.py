"""
cpp_react/router.py — Conditional Routing for C++ ReAct Pipeline
==================================================================

Mirrors nodes/router.py for Java — exact same routing logic.
"""
from __future__ import annotations

from .state import CppReactState


def route_after_test_seq(state: CppReactState) -> str:
    """Route after sequential test: pass → concurrent, fail → reprompt/failure."""
    if state.get("test_result") == "pass":
        return "switch_to_concurrent"

    # Sequential: up to 3 attempts total (initial + 2 retries)
    max_attempts = 3
    attempts = state.get("seq_attempt_count", 0)

    if attempts >= max_attempts:
        return "log_failure"
    return "prepare_reprompt_seq"


def route_after_test_conc(state: CppReactState) -> str:
    """
    Route after concurrent test:
    - All pass + lock-free → success
    - Fail → reprompt (with retry limits per stage)
    """
    comp_ok = state.get("compilation_status") == "pass"
    sanity_ok = state.get("consistency_status") == "pass"
    test_ok = state.get("test_result") == "pass"
    lockf = state.get("lock_freedom_status", "none")

    if comp_ok and sanity_ok and test_ok and lockf == "lock-free":
        return "log_success"

    stage = state.get("failure_stage", "none")
    first_retry_used = state.get("first_sanity_retry_used", False)
    second_retry_used = state.get("second_sanity_retry_used", False)

    # Check attempt budget (up to 4 concurrent attempts total)
    conc_attempts = state.get("conc_attempt_count", 0)
    if conc_attempts >= 4:
        return "log_failure"

    # If failed at compile or first sanity
    if stage in ("compile", "first_sanity") or lockf == "none":
        return "prepare_reprompt_con"

    # If failed at second sanity (lockf != "none")
    if stage == "second_sanity":
        return "prepare_reprompt_con"

    return "log_failure"


def route_after_structural_verify(state: CppReactState) -> str:
    """
    Route after structural verification:
    - Pass → test concurrent
    - Fail + not retried → reprompt structural
    - Fail + already retried → failure
    """
    status = state.get("structural_verify_status", "none")
    if status == "pass":
        return "test_conc"

    retry_used = state.get("structural_retry_used", False)
    if not retry_used:
        return "prepare_reprompt_structural"

    return "log_failure"
