from .state import GraphState


def route_after_test_seq(state: GraphState) -> str:
	# Passed: move to concurrent phase
	if state.get("test_result") == "pass":
		return "switch_to_concurrent"
	
	# Failed
	# User requirement: Sequential code generated only once. No retries.
	# So if it fails here (after attempt 1), we fail.
	max_retries = 1 
	attempts = state.get("seq_attempt_count", 0)
	
	if attempts >= max_retries:
		return "log_failure"
	return "prepare_reprompt_seq"


def route_after_test_conc(state: GraphState) -> str:
	"""
	Route after test_conc (concurrent phase).
	- If pass -> log_success.
	- If fail -> check if we should reprompt for compilation/sanity.
	"""
	comp_ok = state.get("compilation_status") == "pass"
	sanity_ok = state.get("sanity_status") == "pass"
	test_ok = state.get("test_result") == "pass"
	lockf = state.get("lock_freedom_status", "none")

	if comp_ok and sanity_ok and test_ok and lockf == "lock-free":
		return "log_success"

	stage = state.get("failure_stage", "none")
	first_retry_used = state.get("first_sanity_retry_used", False)
	second_retry_used = state.get("second_sanity_retry_used", False)

	# If failed before or at first sanity (includes compile)
	if stage in ("compile", "first_sanity") or lockf == "none":
		if not first_retry_used:
			return "prepare_reprompt_con"
		return "log_failure"

	# If failed at second sanity (lockf != "none")
	if stage == "second_sanity" and not second_retry_used:
		return "prepare_reprompt_con"

	return "log_failure"


def route_after_structural_verify(state: GraphState) -> str:
	"""
	Route after structural verification step.
	- If pass -> test_conc.
	- If fail and not yet retried -> prepare_reprompt_structural.
	- If fail and already retried -> log_failure.
	"""
	status = state.get("structural_verify_status", "none")
	if status == "pass":
		return "test_conc"
	
	retry_used = state.get("structural_retry_used", False)
	if not retry_used:
		return "prepare_reprompt_structural"
	
	return "log_failure"
