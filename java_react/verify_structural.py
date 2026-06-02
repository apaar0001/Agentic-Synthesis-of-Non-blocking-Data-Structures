import os
import re
from typing import Dict, Any
from langchain_openai import ChatOpenAI
from langchain_core.messages import SystemMessage, HumanMessage
from .conversation_history import ConversationHistory, extract_text_from_response
from .state import GraphState


_VERIFY_SYSTEM = (
    "You are a code auditor specializing in concurrent data structures. "
    "Your task is to verify if a concurrent implementation correctly audits the sequential code"
)

_VERIFY_USER_TEMPLATE = """\
Compare the following Sequential Java code with the Concurrent Java code and \
determine if the concurrent version correctly implements the same underlying \
structural properties.

Check the Node structure: Is the concurrent Node class having the pointers/fields as the sequential node. Extra fields for concurrency (locks, flags, sentinel nodes, etc.) are allowed as long as they do not change the underlying sequential data structure.

Check the Interface: Does it correctly implement the same ADT interface methods as the sequential code?

NOTE: The traversal or ordering logic of the concurrent code does not matter for the structural verification. We are only concerned with the structure of the node class.

Sequential Code:
{sequential_code}

Concurrent Code to analyze:
{code}

Respond in the following format:
VERDICT: [YES/NO]
REASON: [Brief explanation if NO, otherwise empty. DO NOT use specific data structure names \
like "Binary Search Tree", "BST", "Linked List", etc. Instead, describe the mismatch in terms \
of pointers (e.g., "missing pointers", or members in the node class).]"""



def _get_llm() -> ChatOpenAI:
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    model = os.environ.get("NVIDIA_NIM_MODEL")
    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key,
        model=model,
        temperature=0.1,
        max_tokens=512,
    )


def _write_log(log_file: str, msg: str) -> None:
    continuous_log = "continuous_logs.txt"
    for p in [log_file, continuous_log]:
        try:
            with open(p, "a", encoding="utf-8") as f:
                f.write(msg)
                f.flush()
        except Exception:
            pass


def node_verify_structural(state: GraphState) -> Dict[str, Any]:
    """
    Verify that the concurrent code structurally mirrors the sequential implementation.

    The verification uses its own fresh [system, user] prompt so the LLM auditor
    is not biased by the generation conversation. However, the verdict is appended
    back to the main conversation_history as a brief system note, giving the
    generator full awareness of what the auditor found when it needs to retry.
    """
    verify_status = state.get("structural_verify_status")
    if verify_status == "pass":
        return {"structural_verify_status": "pass"}  # Already passed

    code = state.get("generated_code", "")
    seq_code = state.get("sequential_code", "")
    attempt_no = state.get("conc_attempt_count", 0)
    current_status = state.get("structural_verify_status", "none")
    log_file = state.get("log_file_path") or "continuous_logs.txt"

    if current_status == "fail" and not code:
        return {"structural_verify_status": "fail"}

    # Bypass after structural reprompt (user requirement: only one structural retry)
    if state.get("structural_retry_used", False):
        history_list = list(state.get("conc_attempt_history", []))
        exists = any(h.get("attempt") == attempt_no for h in history_list)
        if not exists:
            history_list.append({
                "attempt": attempt_no,
                "compilation_status": "none",
                "sanity_status": "none",
                "lock_syntax_status": "unknown",
                "lock_freedom_status": "none",
                "structural_verify_status": "pass",
                "test_result": "pass",
                "error_message": "",
                "code": code
            })
            return {"structural_verify_status": "pass", "conc_attempt_history": history_list}
        return {"structural_verify_status": "pass"}

    if not code:
        result = {
            "structural_verify_status": "fail",
            "error_message": "No code generated to verify.",
            "failure_stage": "structural_verify",
        }
        history_list = list(state.get("conc_attempt_history", []))
        history_list.append({
            "attempt": attempt_no,
            "compilation_status": "none",
            "sanity_status": "none",
            "lock_syntax_status": "unknown",
            "lock_freedom_status": "none",
            "structural_verify_status": "fail",
            "test_result": "fail",
            "error_message": result["error_message"],
        })
        result["conc_attempt_history"] = history_list
        return result

    # ── Build a fresh auditor conversation (separate from main gen history) ──
    verifier_messages = [
        SystemMessage(content=_VERIFY_SYSTEM),
        HumanMessage(content=_VERIFY_USER_TEMPLATE.format(
            sequential_code=seq_code,
            code=code,
        )),
    ]

    _write_log(log_file, "--- [STRUCTURAL VERIFICATION] ---\n")

    try:
        llm = _get_llm()
        response = llm.invoke(verifier_messages)
        response_text = extract_text_from_response(response)

        _write_log(log_file, f"Structural Verification Response:\n{response_text}\n===\n\n")

        verdict_match = re.search(r"VERDICT:\s*(YES|NO)", response_text)
        verdict = verdict_match.group(1).strip() if verdict_match else "NO"

        if verdict == "YES":
            result = {"structural_verify_status": "pass"}
            # Add a brief note to main conversation so the generator knows it passed
            conv_history = ConversationHistory.from_dict_list(state.get("conversation_history", []))
            conv_history.add_user(
                "[Structural auditor verdict: PASS — the concurrent code correctly mirrors "
                "the structural properties of the sequential implementation. Proceed.]"
            )
            conv_history.add_assistant("Understood. The structural audit passed.")
            result["conversation_history"] = conv_history.to_dict_list()
        else:
            reason = (
                response_text.split("REASON:")[1].strip()
                if "REASON:" in response_text
                else "Structural properties do not match the sequential implementation."
            )
            result = {
                "structural_verify_status": "fail",
                "test_result": "fail",
                "error_message": f"Structural Verification Failed: {reason}",
                "failure_stage": "structural_verify",
            }
            # Add the failure reason to main conversation so the generator sees WHY it failed
            conv_history = ConversationHistory.from_dict_list(state.get("conversation_history", []))
            conv_history.add_user(
                f"[Structural auditor verdict: FAIL — Reason: {reason}]"
            )
            conv_history.add_assistant(
                "Understood. I will fix the structural issue in my next attempt."
            )
            result["conversation_history"] = conv_history.to_dict_list()

        # Record attempt history
        history_list = list(state.get("conc_attempt_history", []))
        exists = any(h.get("attempt") == attempt_no for h in history_list)
        if not exists:
            if attempt_no == 0:
                attempt_no = 1
            history_list.append({
                "attempt": attempt_no,
                "compilation_status": "none",
                "sanity_status": "none",
                "lock_syntax_status": "unknown",
                "lock_freedom_status": "none",
                "structural_verify_status": result["structural_verify_status"],
                "test_result": result.get("test_result", "fail") if result["structural_verify_status"] == "fail" else "pass",
                "error_message": result.get("error_message", ""),
            })
            result["conc_attempt_history"] = history_list
            result["conc_attempt_count"] = attempt_no

        return result

    except Exception as e:
        error_msg = f"Structural verification error: {str(e)}"
        _write_log(log_file, f"{error_msg}\n")
        return {"structural_verify_status": "fail", "error_message": error_msg}
