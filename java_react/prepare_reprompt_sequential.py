from typing import Dict, Any
from .state import GraphState
from langsmith import traceable


@traceable
def node_prepare_reprompt_seq(state: GraphState) -> Dict[str, Any]:
	"""
	Prepare a slim correction turn for the sequential phase retry.
	The previous code is already in the conversation history (assistant turn),
	so we only need to describe the failure — no code pasting.
	"""
	error = state.get("error_message", "").strip() or "The sanity test did not pass."
	new_prompt = (
		"Your sequential implementation above failed the sanity test.\n"
		f"Error: {error}\n\n"
		"Analyse the error and provide the corrected complete Java file. Output only code."
	)
	return {"current_prompt": new_prompt}
