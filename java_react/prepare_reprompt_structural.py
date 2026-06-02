from typing import Dict, Any
from .state import GraphState


def node_prepare_reprompt_structural(state: GraphState) -> Dict[str, Any]:
    """
    Create a slim structural reprompt turn.
    The sequential code and the previously generated concurrent code are
    already in the conversation history, so we only send the failure reason
    and the correction requirements.
    """
    error_message = state.get("error_message", "The code does not correctly implement the required data structure structure.")
    reason = (
        error_message.split("Structural Verification Failed: ")[1]
        if "Structural Verification Failed: " in error_message
        else error_message
    )

    reprompt = (
        "The concurrent implementation above has a structural mismatch with the sequential version.\n"
        "Verdict: NO\n"
        f"Reason: {reason}\n\n"
        "Fix it. Requirements:\n"
        "- Class name must be ConcurrentDataStructure\n"
        "- Implement the SetADT interface (add, remove, contains)\n"
        "- Use lock-free techniques (CAS, AtomicMarkableReference, etc.)\n"
        "- Ensure linearizability and progress guarantees\n"
        "- Add // Node has been marked immediately after the successful logical deletion "
        "transition (success path only — not on CAS-failure paths, not after physical removal).\n\n"
        "Output ONLY the complete corrected Java code. Do NOT skip any methods. "
        "NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
    )

    return {
        "current_prompt": reprompt,
        "structural_retry_used": True,
        "failure_stage": "structural_verify_reprompt"
    }
