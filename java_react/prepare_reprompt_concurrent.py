from typing import Dict, Any
import os
from pathlib import Path
from .state import GraphState
from langsmith import traceable


def _log_reprompt_choice(state: GraphState, which: str) -> None:
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	msg = f"[CONC] Using Reprompt {which} for failure_stage={state.get('failure_stage', 'none')}\n"
	for p in [log_file, "continuous_logs.txt"]:
		try:
			with open(p, "a", encoding="utf-8") as f:
				f.write(msg)
				f.flush()
		except Exception:
			pass


_REPROMPT_A = {
	"queue": (
		"The concurrent queue implementation given by you before is not linearizable — "
		"enqueue/dequeue counts are inconsistent.\n\n"
		"Revise the implementation. Ensure:\n"
		"- enqueue() atomically links the new node via CAS on the tail.\n"
		"- dequeue() atomically advances the head sentinel via CAS and returns the value.\n"
		"- The // Dequeue victim point comment appears immediately after the successful CAS "
		"that advances the head (success path only, not on failure/retry paths).\n"
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
	"stack": (
		"The concurrent stack implementation given by you before is not linearizable — "
		"push/pop counts are inconsistent.\n\n"
		"Revise the implementation. Ensure:\n"
		"- push() atomically links the new node via CAS on the top pointer.\n"
		"- pop() atomically advances the top pointer via CAS and returns the value.\n"
		"- The // Pop victim point comment appears immediately after the successful CAS "
		"that removes the top node (success path only, not on failure/retry paths).\n"
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
	"set": (
		"The concurrent implementation given by you before is not linearizable — "
		"collective operation counts are inconsistent.\n\n"
		"Revise the implementation. The // Node has been marked comment must appear "
		"immediately after the successful logical deletion transition (success path only).\n"
		"Placement constraints:\n"
		"Do not place this comment before the logical-deletion transition, "
		"on CAS-failure or retry paths, after physical removal or pointer cleanup.\n"
		"The comment must correspond precisely to the linearization point at which "
		"the node is first considered logically deleted in the abstract data structure."
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
}

_REPROMPT_B = {
	"queue": (
		"The concurrent queue implementation given by you before fails when a thread stalls "
		"inside dequeue() — it is NOT lock-free.\n\n"
		"In a lock-free queue, all threads must be able to make progress even when one stalls.\n\n"
		"Common causes:\n"
		"- Missing tail-helping: a thread lagging behind on tail update blocks enqueue progress.\n"
		"- dequeue() not handling the case where tail is behind head — help advance tail first.\n\n"
		"Fix the implementation so stalled threads do not block others.\n\n"
		"Reminder: add // Dequeue victim point immediately after the successful CAS that "
		"advances the head sentinel (success path only — not on retry paths).\n"
		"Provide the complete fixed ConcurrentDataStructure.java."
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
	"stack": (
		"The concurrent stack implementation given by you before fails when a thread stalls "
		"inside pop() — it is NOT lock-free.\n\n"
		"In a lock-free Treiber stack, all threads must be able to make progress even when one stalls.\n\n"
		"Common causes:\n"
		"- Missing CAS retry loop in pop() — must retry until CAS succeeds or stack is empty.\n"
		"- Shared mutable state other than the top AtomicReference.\n\n"
		"Fix the implementation so stalled threads do not block others.\n\n"
		"Reminder: add // Pop victim point immediately after the successful CAS that removes "
		"the top node (success path only — not on retry paths).\n"
		"Provide the complete fixed ConcurrentDataStructure.java."
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
	"set": (
		"The concurrent implementation given by you before fails when a thread stalls "
		"inside remove() after logical deletion — it is NOT lock-free.\n\n"
		"In a lock-free structure, at least one thread completes its operation "
		"(add/remove/contains) in a finite number of steps.\n\n"
		"Common causes:\n"
		"- Missing helping mechanism: threads encountering a marked-but-unlinked node "
		"must CAS-unlink it before continuing.\n"
		"- Physical cleanup not happening, blocking other threads.\n\n"
		"Fix the implementation above so stalled threads do not block others. "
		"Ensure proper CAS-based physical cleanup where any thread encountering a "
		"logically-deleted node helps unlink it.\n\n"
		"Reminder: add // Node has been marked immediately after logical deletion "
		"(success path only — not on CAS-failure paths, not after physical removal).\n"
		"Provide the complete fixed ConcurrentDataStructure.java."
		"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code."
	),
}


@traceable
def node_prepare_reprompt_con(state: GraphState) -> Dict[str, Any]:
	"""
	Concurrent reprompt node.

	All prior code is already in the conversation history as assistant turns,
	so the reprompt messages are slim correction requests only.

	Reprompt A (compile / first_sanity failures):
	  The code is not linearizable — ask the model to revise.

	Reprompt B (second_sanity / lock-freedom failures):
	  The implementation is not lock-free — explain what's wrong and ask for a fix.
	"""
	failure_stage = state.get("failure_stage", "none")
	ds_type = state.get("data_structure", "set").lower()

	if failure_stage in ("compile", "first_sanity"):
		# ── Reprompt A ────────────────────────────────────────────────────
		_log_reprompt_choice(state, "A")
		new_prompt = _REPROMPT_A.get(ds_type, _REPROMPT_A["set"])
		return {
			"current_prompt": new_prompt,
			"first_sanity_retry_used": True,
		}

	# ── Reprompt B (second_sanity / lock-freedom failure) ─────────────────
	_log_reprompt_choice(state, "B")
	new_prompt = _REPROMPT_B.get(ds_type, _REPROMPT_B["set"])
	return {
		"current_prompt": new_prompt,
		"second_sanity_retry_used": True,
	}
