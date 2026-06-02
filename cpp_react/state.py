"""
cpp_react/state.py — State definition for the C++ ReAct Translation Pipeline
==============================================================================

Mirrors the Java GraphState from nodes/state.py but adapted for C++.
"""
from typing import TypedDict, List, Literal


class CppReactState(TypedDict):
    # ── Identity ───────────────────────────────────────────────────────────────
    prompt_topic: str           # Human-readable prompt name
    original_prompt: str        # Original prompt text from CSV
    current_prompt: str         # Active prompt (may be modified by reprompt nodes)
    data_structure: str         # e.g. "linked_list", "skiplist", "bst", "hash_table"
    prompt_idx: int             # Prompt index (1-based)
    run_idx: int                # Sample index (1-based)

    # ── Generated code ─────────────────────────────────────────────────────────
    generated_code: str         # Current code (latest version)
    sequential_code: str        # Successful sequential code snapshot
    concurrent_code: str        # Concurrent code snapshot
    final_code: str             # Final code after all stages

    # ── Pipeline phase ─────────────────────────────────────────────────────────
    phase: Literal["seq", "conc"]
    seq_attempt_count: int
    conc_attempt_count: int

    # ── Test results ───────────────────────────────────────────────────────────
    test_result: Literal["pass", "fail"]
    compilation_status: Literal["pass", "fail", "none"]
    consistency_status: Literal["pass", "fail", "none"]
    lock_freedom_status: Literal["lock-free", "lock-based", "none", "error"]
    lock_syntax_status: Literal["lock-free", "lock-based", "unknown", "error"]
    error_message: str

    # ── Structural verification ────────────────────────────────────────────────
    structural_verify_status: Literal["pass", "fail", "none"]
    structural_expected: List[str]
    structural_detected: List[str]
    structural_score: float

    # ── Retry tracking ─────────────────────────────────────────────────────────
    failure_stage: Literal["none", "compile", "first_sanity", "second_sanity"]
    first_sanity_retry_used: bool
    second_sanity_retry_used: bool
    compile_retry_used: bool
    structural_retry_used: bool

    # ── Conversation history (stateful LLM context) ────────────────────────────
    conversation_history: List[dict]  # [{"role": "system"|"user"|"assistant", "content": "..."}]

    # ── Logging ────────────────────────────────────────────────────────────────
    log_file_path: str
    final_logs: List[str]
    _last_logged_key: str
