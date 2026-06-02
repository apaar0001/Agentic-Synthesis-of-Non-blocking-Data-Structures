from typing import Dict, Any
import os
from datetime import datetime
from langsmith import traceable
from langchain_openai import ChatOpenAI

from .state import GraphState
from .conversation_history import ConversationHistory, extract_text_from_response
from pathlib import Path
import re


# Shared system prompt (also defined in generate_sequential.py — kept in sync)
SYSTEM_PROMPT = (
    "You are a senior concurrent systems engineer and researcher. "
    "You specialize in transalating given sequential implementation to non-blocking or lock-free data structures "
    "linearizability, memory models, and thread-safety correctness. "
    "When asked to implement a concurrent data structure, implement the full code in Java.\n"
    "- Use production-quality style\n"
    "- Avoid race conditions and deadlocks\n"
    "- Handle edge cases"
	"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code "
)

# Lock-free theory blocks — one per DS type, injected as Turn 6 (conc attempt 1) user message

_CONC_THEORY_SET = """\
Now convert the sequential implementation (JAVA code) you wrote above into a lock-free/non-blocking concurrent version using CAS.

Output Requirement: Provide only the concurrent code implementation with the class name ConcurrentDataStructure. No explanations, no extra text.
The ConcurrentDataStructure class MUST have a no-argument constructor (e.g., `public ConcurrentDataStructure()`).

Lock-Free Implementation Instructions:
No Locks Allowed
Do not use synchronized blocks or ReentrantLock.

CAS-based Updates:
Use compareAndSet operations to atomically insert or delete nodes.
Ensure retry loops handle contention when multiple threads attempt to modify the same pointer.

Logical vs Physical Deletion:
For deletion, first logically mark a node as deleted (using AtomicMarkableReference).
Later, physically remove the node from the structure when it is safe to do so.
IMPORTANT: When using AtomicMarkableReference, never use .get(null). If you want to get the reference without the mark, use .getReference(). Using .get(null) will cause a NullPointerException in Java.

Linearizability Requirement
Every operation (add, remove, contains) must appear as if it takes effect at a single instant.
Readers must not return logically deleted nodes as valid.
When traversing the structure in add(), remove(), or contains(), you MUST skip or physically remove (help) any logically deleted (marked) nodes. Do not use a marked node as a predecessor/parent (prev) for any CAS update. If you find a marked node, either use CAS to unlink it (helping) or skip to its successor/child and retry the search from the last valid node.
If a node is already marked during a remove() operation, that operation should return false immediately.
Ensure all retry loops have a clear path to progress and avoid "restart from root/head" patterns unless the structure has fundamentally changed.

Structural Requirement:
The concurrent Node class must have the pointers/fields as in the sequential Node class. Extra fields for concurrency (locks, flags, sentinel nodes, etc.) are allowed as long as they do not change the underlying sequential data structure.
NOTE: The traversal or ordering logic of the concurrent code does not matter. We are only concerned with the structure of the node class.

Task
Transform the sequential implementation into a lock-free/non-blocking concurrent implementation in Java named ConcurrentDataStructure.
Maintain lock-freedom (progress is guaranteed even if one thread is stalled).
Ensure linearizability (operations must appear atomic and consistent).
Avoid deadlocks and starvation by relying solely on atomic CAS loops.

IMPORTANT:
Across all operations (add, remove, and/or contains), identify the exact point at which a node's abstract state transitions from logically present to logically absent (for example, when a node becomes marked, flagged, or otherwise considered logically deleted), regardless of which method performs or observes this transition.
This transition may occur in:

1) remove() (typical case),
2) contains() (e.g., observing and validating a marked node),
3) or add() (e.g., detecting a logically deleted node during traversal).

Immediately after the successful operation or observation that establishes this logical deletion state (and only on the success path), add the following comment:
// Node has been marked
Placement constraints:
Do not place this comment before the logical-deletion transition, on CAS-failure or retry paths, after physical removal or pointer cleanup.
The comment must correspond precisely to the linearization point at which the node is first considered logically deleted in the abstract data structure.
NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or comments in the code (except the above mentioned comment).

API Contract (SetADT Interface):
package com.example.utils;
/***
 * A simple set interface working with integer keys.
 */
public interface SetADT {
\tboolean add(int key);
\tboolean remove(int key);
\tboolean contains(int key);
}
Ensure that the generated code implements the SetADT interface.\
"""

_CONC_THEORY_QUEUE = """\
Now convert the sequential queue implementation (JAVA code) you wrote above into a lock-free/non-blocking concurrent version using CAS (Michael-Scott Queue style).

Output Requirement: Provide only the concurrent code implementation with the class name ConcurrentDataStructure. No explanations, no extra text.
The ConcurrentDataStructure class MUST have a no-argument constructor (e.g., `public ConcurrentDataStructure()`).

Lock-Free Implementation Instructions:
No Locks Allowed
Do not use synchronized blocks or ReentrantLock.

Algorithm — Michael-Scott Lock-Free Queue:
Use two AtomicReference fields: head and tail, both pointing to Node objects.
Initialize with a sentinel (dummy) node: head = tail = new Node(-1).
enqueue(int val): Atomically swing the tail pointer forward using CAS. Help lagging tail pointers.
dequeue(): Atomically swing the head pointer forward using CAS. Return -1 if the queue is logically empty (head.next == null after sentinel).
isEmpty(): Return true when head.get().next == null (the sentinel has no successor).

Structural Requirement:
The Node class must hold an int value field and an AtomicReference<Node> next field (matching the sequential node structure). No AtomicMarkableReference needed.

Linearizability Requirement:
Every enqueue/dequeue must appear to take effect at a single instant.
The linearization point of dequeue() is the successful CAS that advances the head sentinel.
Ensure all CAS retry loops make progress and never spin indefinitely on a fixed state.

IMPORTANT — Linearization Comment:
Inside dequeue(), immediately after the successful CAS that advances the head (the line that atomically removes the front node), add exactly this comment on the next line:
// Dequeue victim point
Do NOT place this comment on a failure path, inside a retry loop body before the CAS, or after any unrelated statement.
NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or comments in the code (except the above mentioned comment).

API Contract (QueueADT Interface):
package com.example.utils;
public interface QueueADT {
\tvoid enqueue(int val);
\tint dequeue();     // returns -1 if empty
\tboolean isEmpty();
}
Ensure that the generated code implements the QueueADT interface.\
"""

_CONC_THEORY_STACK = """\
Now convert the sequential stack implementation (JAVA code) you wrote above into a lock-free/non-blocking concurrent version using CAS (Treiber Stack style).

Output Requirement: Provide only the concurrent code implementation with the class name ConcurrentDataStructure. No explanations, no extra text.
The ConcurrentDataStructure class MUST have a no-argument constructor (e.g., `public ConcurrentDataStructure()`).

Lock-Free Implementation Instructions:
No Locks Allowed
Do not use synchronized blocks or ReentrantLock.

Algorithm — Treiber Lock-Free Stack:
Use a single AtomicReference<Node> top field initialized to null.
push(int val): Create a new Node, set its next to the current top, then CAS top from the old value to the new node. Retry on CAS failure.
pop(): Read top. If null, return -1. Otherwise CAS top from the current node to current.next. Retry on CAS failure. Return the value of the successfully removed node.
isEmpty(): Return top.get() == null.

Structural Requirement:
The Node class must hold an int value/key field and a Node next field (matching the sequential node structure). No AtomicMarkableReference needed.

Linearizability Requirement:
Every push/pop must appear to take effect at a single instant.
The linearization point of pop() is the successful CAS that removes the top node.
Ensure all CAS retry loops make progress and never spin indefinitely on a fixed state.

IMPORTANT — Linearization Comment:
Inside pop(), immediately after the successful CAS that advances the top pointer (the line that atomically removes the top node), add exactly this comment on the next line:
// Pop victim point
Do NOT place this comment on a failure path, inside a retry loop body before the CAS, or after any unrelated statement.
NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or comments in the code (except the above mentioned comment).

API Contract (StackADT Interface):
package com.example.utils;
public interface StackADT {
\tvoid push(int val);
\tint pop();         // returns -1 if empty
\tboolean isEmpty();
}
Ensure that the generated code implements the StackADT interface.\
"""

# Map ds_type -> theory prompt
_CONC_THEORY_BY_DS = {
    "queue": _CONC_THEORY_QUEUE,
    "stack": _CONC_THEORY_STACK,
}


def _extract_java_code(generated_code: str) -> str:
	"""Extract Java code from generated text, removing markdown formatting."""
	lines = generated_code.split('\n')
	java_lines = []
	in_java_block = False
	
	for line in lines:
		if line.strip().startswith('```java'):
			in_java_block = True
			continue
		elif line.strip() == '```' and in_java_block:
			break
		elif in_java_block:
			java_lines.append(line)
		elif not in_java_block and (line.strip().startswith('package ') or 
									line.strip().startswith('import ') or
									line.strip().startswith('public class ') or
									line.strip().startswith('class ') or
									line.strip().startswith('interface ')):
			java_lines.append(line)
			in_java_block = True
	
	return '\n'.join(java_lines)


def _validate_and_fix_concurrent_code(java_code: str, ds_type: str = "set") -> str:
	"""Validate and fix package/import statements for concurrent code."""
	# Fix AtomicMarkableReference.get(null) hallucination
	java_code = java_code.replace(".get(null)", ".getReference()")

	# Determine the correct ADT interface for this DS type
	adt_map = {
		"queue": ("QueueADT", "com.example.utils.QueueADT"),
		"stack": ("StackADT", "com.example.utils.StackADT"),
	}
	adt_class, adt_import = adt_map.get(ds_type, ("SetADT", "com.example.utils.SetADT"))

	lines = java_code.split('\n')
	fixed_lines = []

	package_found = False
	adt_import_found = False

	for line in lines:
		if line.strip().startswith('package '):
			fixed_lines.append('package com.example.Sets;')
			package_found = True
		elif line.strip().startswith('import ') and adt_class in line:
			fixed_lines.append(line)
			adt_import_found = True
		else:
			fixed_lines.append(line)

	if not package_found:
		fixed_lines.insert(0, 'package com.example.Sets;')

	if not adt_import_found:
		for i, line in enumerate(fixed_lines):
			if line.strip().startswith('package '):
				fixed_lines.insert(i + 1, f'import {adt_import};')
				break

	return '\n'.join(fixed_lines)


def _get_llm() -> ChatOpenAI:
	api_key = os.environ.get("NVIDIA_NIM_API_KEY")
	if not api_key:
		print("Warning: NVIDIA_NIM_API_KEY not found in environment")
	
	model = os.environ.get("NVIDIA_NIM_MODEL")
	print(f"Using NVIDIA NIM model: {model}")
	
	return ChatOpenAI(
		base_url="https://integrate.api.nvidia.com/v1",
		api_key=api_key,
		model=model,
		temperature=0.5,
		max_tokens=8000
	)


@traceable
def node_generate_con(state: GraphState) -> Dict[str, Any]:
	attempt = state.get("conc_attempt_count", 0) + 1
	llm = _get_llm()

	# ── Restore conversation history ───────────────────────────────────────
	history = ConversationHistory.from_dict_list(state.get("conversation_history", []))

	# Ensure system message is present (idempotent — add only if missing)
	history.add_system(SYSTEM_PROMPT)

	# ── Determine DS type ──────────────────────────────────────────────────
	ds_type = state.get("data_structure", "set").lower()

	# ── Build the user turn for this attempt ───────────────────────────────
	if state.get("conc_attempt_count", 0) == 0:
		# Attempt 1: inject the lock-free theory as the next user turn.
		# The sequential code is already in the history (Turn 3 assistant).
		user_prompt = _CONC_THEORY_BY_DS.get(ds_type, _CONC_THEORY_SET)
	else:
		# Retry: the reprompt node has already built state["current_prompt"]
		# which is a slim correction request (no prev_code pasted in).
		user_prompt = state["current_prompt"]

	history.add_user(user_prompt)

	# ── Logging ────────────────────────────────────────────────────────────
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	continuous_log = "continuous_logs.txt"
	timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
	last_logged_key = state.get("_last_logged_key")
	current_key = f"{state['prompt_topic']}|{attempt}"

	if last_logged_key != current_key:
		for log_path in [log_file, continuous_log]:
			with open(log_path, "a", encoding="utf-8") as f:
				f.write(f"--- [CONCURRENT PHASE] Attempt {attempt} ---\n")
				f.write(f"Timestamp: {timestamp_now}\n")
				f.write(f"History turns sent to LLM: {len(history)}\n")
				f.flush()

	# ── LLM call (stateful — full history) ────────────────────────────────
	try:
		response = llm.invoke(history.to_langchain_messages())
		code = extract_text_from_response(response)
	except Exception as e:
		for log_path in [log_file, continuous_log]:
			with open(log_path, "a", encoding="utf-8") as f:
				f.write(f"ERROR: LLM API call failed: {str(e)}\n")
				f.write(f"Error type: {type(e).__name__}\n")
				f.flush()
		raise

	# ── Store assistant response ───────────────────────────────────────────
	history.add_assistant(code)

	# ── Extract and validate Java code ────────────────────────────────────
	java_code = _extract_java_code(code)
	fixed_java_code = _validate_and_fix_concurrent_code(java_code, ds_type=ds_type)

	is_empty = len(fixed_java_code.strip()) < 100 or "ConcurrentDataStructure" not in fixed_java_code

	for log_path in [log_file, continuous_log]:
		with open(log_path, "a", encoding="utf-8") as f:
			f.write("MODEL OUTPUT:\n")
			f.write(fixed_java_code + "\n")
			if is_empty:
				f.write("WARNING: Model output appears empty or invalid. Failing attempt.\n")
			f.write("===\n\n")
			f.flush()

	if is_empty:
		return {
			"generated_code": "",
			"conc_attempt_count": attempt,
			"_last_logged_key": current_key,
			"structural_verify_status": "fail",
			"error_message": "LLM failed to generate a valid class implementation.",
			"failure_stage": "structural_verify",
			"conversation_history": history.to_dict_list(),
		}

	return {
		"generated_code": fixed_java_code,
		"conc_attempt_count": attempt,
		"_last_logged_key": current_key,
		"conversation_history": history.to_dict_list(),
	}
