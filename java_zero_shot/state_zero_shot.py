"""
State definition for the zero-shot pipeline.

ZeroShotState is compatible with GraphState for all fields that the existing
test nodes (node_test_code_conc, node_prepare_reprompt_con, etc.) access,
so those nodes can be reused without modification.
"""
from typing import TypedDict, List, Literal


class ZeroShotState(TypedDict):
    # ── Identity ────────────────────────────────────────────────────────────
    ds_name: str            # e.g. "linked_list", "binary_search_tree"
    prompt_idx: int         # which zero-shot prompt variant (1-indexed)
    run_idx: int            # sample number within this prompt

    # ── Prompt / generation ─────────────────────────────────────────────────
    prompt_topic: str       # human label (same as ds_name typically)
    original_prompt: str    # the initial zero-shot prompt text
    current_prompt: str     # may be overwritten by reprompt nodes
    generated_code: str     # current candidate Java code

    # ── Conversation history (stateful across retries) ─────────────────────
    conversation_history: List[dict]  # [{role, content}, ...]

    # ── Metrics (written by test nodes) ────────────────────────────────────
    compilation_status: Literal["pass", "fail", "none"]
    sanity_status: Literal["pass", "fail", "none"]
    lock_freedom_status: Literal["lock-free", "lock-based", "none", "error"]
    lock_syntax_status: Literal["lock-free", "lock-based", "unknown", "error"]
    test_result: Literal["pass", "fail"]
    error_message: str

    # ── Attempt tracking ────────────────────────────────────────────────────
    conc_attempt_count: int
    max_retries: int
    conc_attempt_history: List[dict]  # one entry per attempt
    failure_stage: Literal["none", "compile", "first_sanity", "second_sanity", "ds_judge"]
    first_sanity_retry_used: bool
    second_sanity_retry_used: bool
    compile_retry_used: bool

    # ── Fields used by shared test_code nodes (must exist in state) ────────
    phase: Literal["conc"]          # always "conc" for zero-shot
    sequential_code: str            # empty string (no sequential phase)
    data_structure: str             # alias for ds_name (used by utils.py)
    final_logs: List[str]
    prompt_name: str
    log_file_path: str
    _last_logged_key: str
    asked_human: bool
    last_human_feedback: str
    human_feedback_count: int
    human_feedback_1: str
    human_feedback_2: str
    final_code: str

    # ── DS Judge (zero-shot only) ────────────────────────────────────────────
    ds_judge_status:  Literal["pass", "fail", "none"]
    ds_judge_verdict: str   # "YES", "NO", "ERROR", or "" if not yet run
    ds_judge_reason:  str   # one-sentence reason from the judge LLM

    # ── Structural fields (not used in zero-shot but required by state) ─────
    structural_expected: List[str]
    structural_detected: List[str]
    structural_score: float
    structural_verify_status: Literal["pass", "fail", "none"]
    structural_retry_used: bool

    # ── Code snapshots injected by test_code.py ────────────────────────────
    first_sanity_code: str
    victim_injected_code: str

    # ── Approach tag (used by extended_codebleu.py to label results) ───────
    approach: Literal["zero_shot"]
