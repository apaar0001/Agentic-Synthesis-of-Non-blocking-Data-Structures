from typing import TypedDict, List, Literal, Optional


class GraphState(TypedDict):
	prompt_topic: str
	original_prompt: str
	current_prompt: str
	generated_code: str
	test_result: Literal["pass", "fail"]
	error_message: str
	seq_attempt_count: int
	conc_attempt_count: int
	max_retries: int  # kept for compatibility; not used in new conc flow
	final_logs: List[str]
	# per-prompt logging fields
	prompt_name: str
	log_file_path: str
	# internal guard to avoid duplicate logging per attempt
	_last_logged_key: str
	# pipeline phase and prompts
	phase: Literal["seq", "conc"]
	asked_human: bool
	sequential_code: str
	concurrent_code: str
	# Metrics tracking
	compilation_status: Literal["pass", "fail", "none"]
	sanity_status: Literal["pass", "fail", "none"]
	lock_freedom_status: Literal["lock-free", "lock-based", "none", "error"]
	lock_syntax_status: Literal["lock-free", "lock-based", "unknown", "error"]
	last_human_feedback: str
	human_feedback_count: int
	# Code storage fields
	data_structure: str
	prompt_idx: int
	run_idx: int
	# Attempt history for concurrent phase (list of dicts)
	conc_attempt_history: List[dict]
	# Human feedback permanent storage
	human_feedback_1: str
	human_feedback_2: str
	# Final code snapshot after feedback loop
	final_code: str
	# Structural correctness fields
	structural_expected: List[str]
	structural_detected: List[str]
	structural_score: float
	# Concurrent failure / retry tracking
	failure_stage: Literal["none", "compile", "first_sanity", "second_sanity"]
	first_sanity_retry_used: bool
	second_sanity_retry_used: bool
	compile_retry_used: bool
	# Structural Verification
	structural_retry_used: bool
	structural_verify_status: Literal["pass", "fail", "none"]
	# Code snapshots for storage
	first_sanity_code: str
	victim_injected_code: str
	# Stateful conversation history (grows across the whole sample run; reset per run_idx)
	conversation_history: List[dict]  # [{"role": "system"|"user"|"assistant", "content": "..."}]
