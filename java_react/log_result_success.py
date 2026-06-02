from typing import Dict, Any
from datetime import datetime
from .state import GraphState
import os
from pathlib import Path


from .utils import save_generated_code


def node_log_success(state: GraphState) -> Dict[str, Any]:
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	continuous_log = "continuous_logs.txt"
	timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
	phase = state.get("phase", "unknown")
	if phase == "seq":
		attempt = state.get("seq_attempt_count", 0)
	else:
		attempt = state.get("conc_attempt_count", 0)
	hf1 = state.get("human_feedback_1", "")
	hf2 = state.get("human_feedback_2", "")
	
	# Save sequential code
	seq_code = state.get("sequential_code")
	if seq_code:
		save_generated_code(seq_code, state, "SequentialDataStructure.java")

	# Save concurrent code after first sanity
	conc_sanity1 = state.get("first_sanity_code")
	if conc_sanity1:
		save_generated_code(conc_sanity1, state, "ConcurrentDataStructure_Sanity1.java")
	
	# Save concurrent code tested in non-blocking test (victim injected)
	conc_victim = state.get("victim_injected_code")
	if conc_victim:
		save_generated_code(conc_victim, state, "ConcurrentDataStructure_Victim.java")
	
	# Save concurrent code when it passes (final successful code)
	if phase == "conc":
		concurrent_code = state.get("final_code") or state.get("generated_code", "")
		if concurrent_code:
			save_generated_code(concurrent_code, state, "ConcurrentDataStructure.java")
	
	# Save the continuous log for this sample
	from .utils import save_continuous_log
	save_continuous_log(state)

	header = "SUCCESS"
	if phase == "conc":
		header = "COMPLETED"

	message = (f"=== [{header}] ===\n"
	           f"Timestamp:          {timestamp_now}\n"
	           f"Phase:              {phase.upper()}\n"
	           f"Attempt:            {attempt}\n"
	           f"Structural Status:  {state.get('structural_verify_status', 'unknown')}\n"
	           f"Compilation Status: {state.get('compilation_status', 'unknown')}\n"
	           f"Sanity Status:      {state.get('sanity_status', 'unknown')}\n")
	if phase == "conc":
		message += f"Lock-Freedom Status: {state.get('lock_freedom_status', 'none')}\n"
		if hf1:
			message += f"Human Feedback 1:   {hf1}\n"
		if hf2:
			message += f"Human Feedback 2:   {hf2}\n"
	message += (f"Prompt Topic:       {state.get('prompt_topic', 'unknown')}\n"
	           f"=== END {header} ===\n\n")
	
	for log_path in [log_file, continuous_log]:
		with open(log_path, "a", encoding="utf-8") as f:
			f.write(message)
			f.flush()
	
	return {"test_result": state.get("test_result", "pass")}


