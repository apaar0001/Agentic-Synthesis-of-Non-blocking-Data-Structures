"""
cpp_react/generate_concurrent.py — Concurrent C++ Generation Node
====================================================================

Mirrors nodes/generate_concurrent.py for Java.
Translates sequential C++ → lock-free concurrent C++ using CAS operations.
Uses ConversationHistory for stateful multi-turn interaction.
"""
from __future__ import annotations

import os
import re
from typing import Dict, Any
from datetime import datetime
from pathlib import Path

from langchain_openai import ChatOpenAI

import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from nodes.conversation_history import ConversationHistory, extract_text_from_response

from .state import CppReactState


# ── System prompt ─────────────────────────────────────────────────────────────

SYSTEM_PROMPT = (
    "You are a senior concurrent systems engineer and researcher. "
    "You specialize in translating sequential implementations to "
    "non-blocking or lock-free data structures in C++17 using std::atomic "
    "and CAS operations. "
    "You understand linearizability, memory models, and thread-safety correctness.\n"
    "- Use production-quality C++17 style\n"
    "- Use std::atomic<T*> with compare_exchange_strong/weak\n"
    "- Use explicit memory ordering (acquire/release/acq_rel/relaxed)\n"
    "- Avoid ALL locking primitives (std::mutex, std::lock_guard, etc.)\n"
    "- Handle edge cases\n"
    "NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text "
    "or long comments in the code."
)


# ── Per-DS lock-free theory prompts (C++ versions of Java _CONC_THEORY_*) ────

_CONC_THEORY_SET = """\
Now convert the sequential C++ implementation you wrote above into a lock-free/non-blocking concurrent version using CAS.

Output Requirement: Provide only the concurrent code implementation with the class name ConcurrentDataStructure. No explanations, no extra text.
The ConcurrentDataStructure class MUST inherit from SetADT and override all three methods: contains(), add(), remove().
The class MUST have a no-argument constructor.

Lock-Free C++17 Implementation Instructions:

1. NO LOCKS: Do not use std::mutex, std::lock_guard, std::unique_lock, or any blocking primitives.

2. ATOMIC OPERATIONS: Use std::atomic<T*> for all shared pointer fields in the Node struct.
   Use compare_exchange_strong() for atomic updates on whichever atomic pointer fields connect nodes together in your structure.

3. LOGICAL vs PHYSICAL DELETION using Pointer Tagging:
   Since C++ does not have AtomicMarkableReference, use the lowest bit of stored pointer values to represent the logical deletion mark.
   Write static helper functions (is_marked_ref, get_unmarked_ref, get_marked_ref) that use reinterpret_cast<uintptr_t> to set, clear, and test the lowest bit.
   CRITICAL: The marked bit lives inside the value stored in an atomic pointer field, NOT in the memory address of the node itself. When checking whether a node is logically deleted, you must read the raw pointer from the atomic field and test its lowest bit BEFORE stripping it. Never strip the mark bit first and then test — that will always evaluate to false.

4. MEMORY ORDERING: Use explicit memory ordering for all atomic operations:
   - std::memory_order_acquire for loads
   - std::memory_order_release for stores
   - std::memory_order_acq_rel for read-modify-write operations (CAS)
   - std::memory_order_relaxed for operations that don't need synchronization

5. DELETION PROTOCOL: For remove(), first logically mark the node by CAS-ing the appropriate outgoing pointer(s) to set the lowest bit, then physically unlink the node from the structure.

6. STRUCTURE TOPOLOGY: Preserve the same topology as the sequential version. Use appropriate sentinel or dummy nodes if the sequential version uses them.

7. SINGLE-FILE .hpp: Include #pragma once, <atomic>, <climits>, <cstdint>, "../utils/SetADT.hpp".

8. CONSTRUCTOR: Include a no-argument constructor.

Structural Requirement:
The concurrent Node struct must have the same pointer fields as in the sequential Node struct, but wrapped in std::atomic<>. Extra fields for concurrency (flags, sentinel nodes, etc.) are allowed as long as they do not change the underlying sequential data structure's topology.

ALGORITHMIC PITFALLS:
- When traversing the structure in add(), remove(), or contains(), you MUST skip or physically remove (help) any logically deleted (marked) nodes. Do not use a marked node as a predecessor/parent for any CAS update. If you find a marked node, either CAS to unlink it (helping) or skip to its successor/child and retry from the last valid node.
- If a node is already marked during a remove() operation, that operation should return false immediately.
- If a compare_exchange_* fails during physical unlinking, or your predecessor/parent pointer becomes stale, you must safely restart the traversal from the root of the structure. Do NOT simply break an inner loop and continue with stale or disconnected pointers — this causes Segmentation Faults.
- Do NOT manually call `delete` on physically unlinked nodes during concurrent execution. A stale thread may still be reading the node. It is acceptable to leak unlinked nodes for strict lock-freedom correctness in this benchmark.

Linearizability Requirement:
Every operation (add, remove, contains) must appear as if it takes effect at a single instant.
Readers must not return logically deleted nodes as valid.

IMPORTANT — Linearization Comment:
Across all operations (add, remove, and/or contains), identify the exact point at which a node's abstract state transitions from logically present to logically absent (e.g., when its outgoing pointer is successfully marked via CAS).
Immediately after the successful CAS that establishes this logical deletion, add exactly this comment:
// Node has been marked
Do not place this comment before the marking CAS, on failure/retry paths, or after physical unlinking.

NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or comments (except the above).
Output ONLY the C++ code in a single .hpp file.\
"""

_CONC_THEORY_BY_DS = {}  # All DS types use _CONC_THEORY_SET for C++ (Set ADT interface)


# ── Code helpers ──────────────────────────────────────────────────────────────
import time

def _call_llm_with_retry(llm, messages, log_file: str, max_retries: int = 5, base_delay: float = 10.0):
    """Invoke LLM with exponential backoff retry on 504 / transient errors."""
    for attempt in range(1, max_retries + 1):
        try:
            return llm.invoke(messages)
        except Exception as e:
            err_str = str(e)
            is_transient = (
                "504" in err_str
                or "502" in err_str
                or "503" in err_str
                or "Gateway" in err_str
                or "timeout" in err_str.lower()
                or "connection" in err_str.lower()
            )
            if is_transient and attempt < max_retries:
                delay = base_delay * (2 ** (attempt - 1))   # 10s, 20s, 40s …
                with open(log_file, "a", encoding="utf-8") as f:
                    f.write(
                        f"WARNING: Transient error on attempt {attempt}/{max_retries}: "
                        f"{err_str}\n"
                        f"Retrying in {delay:.0f}s …\n"
                    )
                    f.flush()
                time.sleep(delay)
            else:
                raise   # non-transient, or exhausted retries
            
def _extract_cpp_code(generated_code: str) -> str:
    """Extract C++ code from generated text, removing markdown formatting."""
    lines = generated_code.split('\n')
    cpp_lines = []
    in_block = False

    for line in lines:
        if line.strip().startswith('```cpp') or line.strip().startswith('```c++'):
            in_block = True
            continue
        elif line.strip() == '```' and in_block:
            break
        elif in_block:
            cpp_lines.append(line)
        elif not in_block and (
            line.strip().startswith('#pragma once') or
            line.strip().startswith('#include') or
            line.strip().startswith('class ') or
            line.strip().startswith('struct ')
        ):
            cpp_lines.append(line)
            in_block = True

    return '\n'.join(cpp_lines)


def _validate_and_fix_concurrent_code(code: str) -> str:
    """Validate and fix concurrent C++ code structure."""
    # Ensure #pragma once
    if '#pragma once' not in code:
        code = '#pragma once\n' + code

    # Ensure class name
    if 'ConcurrentDataStructure' not in code:
        code = re.sub(
            r'class\s+(\w+)',
            'class ConcurrentDataStructure',
            code,
            count=1,
        )

    # Ensure SetADT include
    if 'SetADT' not in code and '#include' in code:
        # Insert after #pragma once
        code = code.replace(
            '#pragma once',
            '#pragma once\n#include "../utils/SetADT.hpp"',
            1,
        )

    # Ensure <atomic> include
    if '<atomic>' not in code:
        code = code.replace(
            '#pragma once',
            '#pragma once\n#include <atomic>',
            1,
        )

    return code


# ── LLM factory ───────────────────────────────────────────────────────────────

def _get_llm() -> ChatOpenAI:
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    if not api_key:
        raise ValueError("NVIDIA_NIM_API_KEY not set")

    model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")

    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key,
        model=model,
        temperature=0.5,
        max_tokens=8000,
    )


# ── Node function ─────────────────────────────────────────────────────────────

def node_generate_con(state: CppReactState) -> Dict[str, Any]:
    """Generate lock-free C++ from sequential code using conversation history."""
    attempt = state.get("conc_attempt_count", 0) + 1
    llm = _get_llm()

    # ── Restore conversation history
    history = ConversationHistory.from_dict_list(
        state.get("conversation_history", [])
    )
    history.add_system(SYSTEM_PROMPT)

    # ── Build the user turn
    if state.get("conc_attempt_count", 0) == 0:
        # Attempt 1: inject the lock-free theory. Sequential code is in history.
        user_prompt = _CONC_THEORY_BY_DS.get(
            state.get("data_structure", "linked_list"),
            _CONC_THEORY_SET,
        )
    else:
        # Retry: the reprompt node has built state["current_prompt"]
        user_prompt = state["current_prompt"]

    history.add_user(user_prompt)

    # ── Logging
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"
    timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    last_logged_key = state.get("_last_logged_key")
    current_key = f"{state.get('prompt_topic', '')}|conc|{attempt}"

    if last_logged_key != current_key:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"--- [CONCURRENT PHASE] Attempt {attempt} ---\n")
            f.write(f"Timestamp: {timestamp_now}\n")
            f.write(f"History turns sent to LLM: {len(history)}\n")
            f.flush()

    # ── LLM call (stateful — full history)
    try:
        llm = _get_llm()  # Refresh LLM instance for each call to mitigate transient issues
        response = _call_llm_with_retry(llm, history.to_langchain_messages(), log_file)
        code = extract_text_from_response(response)
    except Exception as e:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"ERROR: LLM API call failed after retries: {str(e)}\n")
            f.write(f"Error type: {type(e).__name__}\n")
            f.flush()
        raise

    # ── Store assistant response
    history.add_assistant(code)

    # ── Extract and validate C++ code
    cpp_code = _extract_cpp_code(code)
    fixed_code = _validate_and_fix_concurrent_code(cpp_code)

    is_empty = (
        len(fixed_code.strip()) < 100
        or "ConcurrentDataStructure" not in fixed_code
    )

    with open(log_file, "a", encoding="utf-8") as f:
        f.write("MODEL OUTPUT:\n")
        f.write(fixed_code + "\n")
        if is_empty:
            f.write("WARNING: Model output appears empty or invalid.\n")
        f.write("===\n\n")
        f.flush()

    if is_empty:
        return {
            "generated_code": "",
            "conc_attempt_count": attempt,
            "_last_logged_key": current_key,
            "structural_verify_status": "fail",
            "error_message": "LLM failed to generate a valid C++ class.",
            "failure_stage": "compile",
            "conversation_history": history.to_dict_list(),
        }

    return {
        "generated_code": fixed_code,
        "concurrent_code": fixed_code,
        "conc_attempt_count": attempt,
        "_last_logged_key": current_key,
        "conversation_history": history.to_dict_list(),
    }
