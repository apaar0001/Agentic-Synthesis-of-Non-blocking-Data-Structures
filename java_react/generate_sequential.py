from typing import Dict, Any
import os
from datetime import datetime
from langchain_core.output_parsers import StrOutputParser
from langsmith import traceable
from langchain_openai import ChatOpenAI
from .state import GraphState
from .conversation_history import ConversationHistory, extract_text_from_response
from pathlib import Path


# System prompt shared with the concurrent phase — defines the agent's expertise
SYSTEM_PROMPT = (
    "You are a senior concurrent systems engineer and researcher. "
    "You specialize in transalating given sequential implementation to non-blocking or lock-free data structures "
    "linearizability, memory models, and thread-safety correctness. "
    "When asked to implement a data structure, implement the full code in Java.\n"
    "- Use production-quality style\n"
    "- Avoid race conditions and deadlocks\n"
    "- Handle edge cases"
	"NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments in the code "
)


def _extract_java_code(generated_code: str) -> str:
	"""Extract Java code from generated text, removing markdown formatting."""
	lines = generated_code.split('\n')
	java_lines = []
	in_java_block = False
	
	for line in lines:
		# Start of Java code block
		if line.strip().startswith('```java'):
			in_java_block = True
			continue
		# End of code block
		elif line.strip() == '```' and in_java_block:
			break
		# Regular Java code
		elif in_java_block:
			java_lines.append(line)
		# Look for Java code without markdown (fallback)
		elif not in_java_block and (line.strip().startswith('package ') or 
									line.strip().startswith('import ') or
									line.strip().startswith('public class ') or
									line.strip().startswith('class ') or
									line.strip().startswith('interface ')):
			java_lines.append(line)
			in_java_block = True
	
	return '\n'.join(java_lines)


def _validate_and_fix_java_code(java_code: str) -> str:
	"""Validate and fix package/import statements for sequential code."""
	lines = java_code.split('\n')
	fixed_lines = []
	
	# Ensure correct package statement
	package_found = False
	import_found = False
	
	for line in lines:
		if line.strip().startswith('package '):
			# Replace with correct package
			fixed_lines.append('package com.example.Sets;')
			package_found = True
		elif line.strip().startswith('import ') and 'SetADT' in line:
			# Keep existing SetADT import
			fixed_lines.append(line)
			import_found = True
		else:
			fixed_lines.append(line)
	
	# Add missing package if not found
	if not package_found:
		fixed_lines.insert(0, 'package com.example.Sets;')
	
	# Add missing import if not found
	if not import_found:
		# Insert after package statement
		for i, line in enumerate(fixed_lines):
			if line.strip().startswith('package '):
				fixed_lines.insert(i + 1, 'import com.example.utils.SetADT;')
				break
	
	return '\n'.join(fixed_lines)


def _llm() -> ChatOpenAI:
	api_key = os.environ.get("NVIDIA_NIM_API_KEY")
	if not api_key:
		raise ValueError("NVIDIA_NIM_API_KEY environment variable is not set")
	
	model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
	
	return ChatOpenAI(
		base_url="https://integrate.api.nvidia.com/v1",
		api_key=api_key,
		model=model,
		temperature=0.2,
		max_tokens=3000
	)


@traceable
def node_generate_seq(state: GraphState) -> Dict[str, Any]:
	attempt = state.get("seq_attempt_count", 0) + 1

	# ── Restore / extend conversation history ──────────────────────────────
	history = ConversationHistory.from_dict_list(state.get("conversation_history", []))

	# Turn 1 (system) — idempotent; only added once per sample run
	history.add_system(SYSTEM_PROMPT)

	# Build the user turn — either the original CSV prompt or the reprompt
	user_prompt = state.get("current_prompt", "")
	history.add_user(user_prompt)

	# ── Logging ────────────────────────────────────────────────────────────
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	continuous_log = "continuous_logs.txt"
	timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

	for log_path in [log_file, continuous_log]:
		with open(log_path, "a", encoding="utf-8") as f:
			f.write(f"--- [SEQUENTIAL PHASE] Attempt {attempt} ---\n")
			f.write(f"Timestamp: {timestamp_now}\n")
			f.write(f"History turns sent to LLM: {len(history)}\n")
			f.flush()

	# ── LLM call (stateful — full history) ────────────────────────────────
	try:
		llm = _llm()
		response = llm.invoke(history.to_langchain_messages())
		code = extract_text_from_response(response)
	except Exception as e:
		for log_path in [log_file, continuous_log]:
			with open(log_path, "a", encoding="utf-8") as f:
				f.write(f"ERROR: LLM API call failed: {str(e)}\n")
				f.write(f"Error type: {type(e).__name__}\n")
				f.flush()
		raise

	# ── Store assistant response in history ───────────────────────────────
	history.add_assistant(code)

	# ── Extract and validate Java code ────────────────────────────────────
	java_code = _extract_java_code(code)
	fixed_java_code = _validate_and_fix_java_code(java_code)

	for log_path in [log_file, continuous_log]:
		with open(log_path, "a", encoding="utf-8") as f:
			f.write("MODEL OUTPUT:\n")
			f.write(code + "\n")
			f.write("===\n\n")
			f.flush()

	return {
		"generated_code": fixed_java_code,
		"seq_attempt_count": attempt,
		"sequential_code": fixed_java_code,
		"conversation_history": history.to_dict_list(),
	}
