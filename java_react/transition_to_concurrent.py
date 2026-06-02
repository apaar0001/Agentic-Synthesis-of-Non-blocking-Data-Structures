from typing import Dict, Any
from .state import GraphState
from langsmith import traceable
import os
from pathlib import Path


from .utils import save_generated_code


@traceable
def node_switch_to_concurrent(state: GraphState) -> Dict[str, Any]:
	# Reset attempts and switch phase; carry over concurrent prompt if present
	sequential_code = state.get("generated_code", "")
	
	# Save sequential code when it passes (before moving to concurrent phase)
	if sequential_code:
		save_generated_code(sequential_code, state, "SequentialDataStructure.java")
	
	return {
		"phase": "conc",
		"conc_attempt_count": 0,
		"human_feedback_count": 0,
		"conc_attempt_history": [],
		# Seed the conc prompt with theory + the sequential code generated
		"current_prompt": "",
		"sequential_code": sequential_code,
		"compilation_status": "none",
		"sanity_status": "none",
		"structural_verify_status": "none",
		"lock_freedom_status": "none",
		"lock_syntax_status": "unknown",
	}


