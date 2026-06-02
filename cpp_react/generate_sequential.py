"""
cpp_react/generate_sequential.py — Sequential C++ Generation Node
==================================================================

Mirrors nodes/generate_sequential.py for Java.
Generates a sequential (non-concurrent) C++ implementation from the CSV prompt.
Uses ConversationHistory for stateful multi-turn LLM interaction.
"""
from __future__ import annotations

import os
import re
from typing import Dict, Any
from datetime import datetime
from pathlib import Path

from langchain_openai import ChatOpenAI

# Reuse existing conversation history from Java pipeline
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from nodes.conversation_history import ConversationHistory, extract_text_from_response

from .state import CppReactState


# ── System prompt (C++ version of the Java system prompt) ─────────────────────

SYSTEM_PROMPT = (
    "You are a senior concurrent systems engineer and researcher. "
    "You specialize in implementing data structures in C++17. "
    "When asked to implement a data structure, implement the full code in C++. "
    "You produce production-quality C++17 code.\n"
    "- Use production-quality style\n"
    "- Handle edge cases\n"
    "- Include proper memory management (RAII)\n"
    "NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text "
    "or long comments in the code."
)


# ── C++ SetADT interface (same as cpp_zero_shot/cpp_prompts.py) ───────────────

_SET_ADT_INTERFACE = """\
#pragma once

class SetADT {
public:
    virtual ~SetADT() = default;
    virtual bool contains(int key) = 0;
    virtual bool add(int key) = 0;
    virtual bool remove(int key) = 0;
};"""


# ── Per-DS sequential prompts ─────────────────────────────────────────────────

_SEQ_PROMPTS = {
    "linked_list": f"""\
Implement a simple sequential singly linked list in C++17 that implements the following interface:

{_SET_ADT_INTERFACE}

The linked list stores integer keys in sorted order (ascending).

The Node struct must have:
  - An integer val field
  - A Node* next pointer

Requirements:
1. The class MUST be named ConcurrentDataStructure and inherit from SetADT.
2. Include a no-argument constructor that initializes the structure (use sentinel nodes with INT_MIN and INT_MAX).
3. Implement contains(), add(), remove() as simple sequential operations (no atomics, no threads).
4. Include a proper destructor that frees all allocated nodes.
5. Single .hpp file with #pragma once.
6. Include: "../utils/SetADT.hpp", <climits>

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
```

Output ONLY the C++ code. No explanations, no markdown fencing.
""",

    "skiplist": f"""\
Implement a simple sequential skip list in C++17 that implements the following interface:

{_SET_ADT_INTERFACE}

The skip list stores integer keys in sorted order across multiple levels (MAX_LEVEL = 16).

The Node struct must have:
  - An integer val field
  - A Node* forward array for each level
  - A topLevel field indicating the node's height

Requirements:
1. The class MUST be named ConcurrentDataStructure and inherit from SetADT.
2. Include a no-argument constructor with sentinel nodes (INT_MIN, INT_MAX).
3. Implement contains(), add(), remove() as simple sequential operations.
4. Use a random level generator for new nodes.
5. Include a proper destructor.
6. Single .hpp file with #pragma once.

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
#include <random>
```

Output ONLY the C++ code. No explanations, no markdown fencing.
""",

    "bst": f"""\
Implement a simple sequential binary search tree (BST) in C++17 that implements the following interface:

{_SET_ADT_INTERFACE}

The BST must maintain the standard BST invariant: left child key < parent key < right child key.

The Node struct must have:
  - An integer val field
  - A Node* left child pointer
  - A Node* right child pointer

Requirements:
1. The class MUST be named ConcurrentDataStructure and inherit from SetADT.
2. Include a no-argument constructor.
3. Implement contains(), add(), remove() as simple sequential operations.
4. Include a proper destructor that frees all nodes.
5. Single .hpp file with #pragma once.

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
```

Output ONLY the C++ code. No explanations, no markdown fencing.
""",

    "hash_table": f"""\
Implement a simple sequential hash table in C++17 that implements the following interface:

{_SET_ADT_INTERFACE}

Use a fixed-size bucket array with linked lists for collision resolution.

Requirements:
1. The class MUST be named ConcurrentDataStructure and inherit from SetADT.
2. Include a no-argument constructor.
3. Use a fixed BUCKET_COUNT (e.g., 64) and a simple hash function.
4. Each bucket is a sorted linked list.
5. Implement contains(), add(), remove() as simple sequential operations.
6. Include a proper destructor.
7. Single .hpp file with #pragma once.

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
```

Output ONLY the C++ code. No explanations, no markdown fencing.
""",
}

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

def _get_seq_prompt(ds_name: str) -> str:
    """Get the sequential prompt for a given data structure, falling back to linked_list."""
    # Normalize DS name
    ds_lower = ds_name.lower().replace("-", "_").replace(" ", "_")
    # Map common aliases
    aliases = {
        "binary_search_tree": "bst",
        "b_minus_tree": "bst",  # approximate with BST
        "hashtable": "hash_table",
        "skip_list": "skiplist",
    }
    ds_key = aliases.get(ds_lower, ds_lower)
    return _SEQ_PROMPTS.get(ds_key, _SEQ_PROMPTS["linked_list"])


# ── Code extraction helpers ───────────────────────────────────────────────────

def _extract_cpp_code(generated_code: str) -> str:
    """Extract C++ code from generated text, removing markdown formatting."""
    lines = generated_code.split('\n')
    cpp_lines = []
    in_cpp_block = False

    for line in lines:
        if line.strip().startswith('```cpp') or line.strip().startswith('```c++'):
            in_cpp_block = True
            continue
        elif line.strip() == '```' and in_cpp_block:
            break
        elif in_cpp_block:
            cpp_lines.append(line)
        elif not in_cpp_block and (
            line.strip().startswith('#pragma once') or
            line.strip().startswith('#include') or
            line.strip().startswith('class ') or
            line.strip().startswith('struct ') or
            line.strip().startswith('template')
        ):
            cpp_lines.append(line)
            in_cpp_block = True

    return '\n'.join(cpp_lines)


def _validate_and_fix_sequential_code(code: str) -> str:
    """Validate and fix sequential C++ code structure."""
    # Ensure #pragma once at top
    if '#pragma once' not in code:
        code = '#pragma once\n' + code

    # Ensure class name is ConcurrentDataStructure (even for sequential)
    # This ensures compatibility with the test harness
    if 'ConcurrentDataStructure' not in code:
        # Try to find any class name and rename it
        code = re.sub(
            r'class\s+(\w+)',
            'class ConcurrentDataStructure',
            code,
            count=1,
        )

    return code


# ── LLM factory ───────────────────────────────────────────────────────────────

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
        max_tokens=4000,
    )


# ── Node function ─────────────────────────────────────────────────────────────

def node_generate_seq(state: CppReactState) -> Dict[str, Any]:
    """Generate sequential C++ implementation from CSV prompt."""
    attempt = state.get("seq_attempt_count", 0) + 1

    # ── Restore / extend conversation history
    history = ConversationHistory.from_dict_list(
        state.get("conversation_history", [])
    )

    # Turn 1 (system) — idempotent; only added once per sample run
    history.add_system(SYSTEM_PROMPT)

    # Build the user turn — either the original CSV prompt or the reprompt
    user_prompt = state.get("current_prompt", "")
    # On first attempt, use proper C++ sequential prompt (ignore Java CSV text)
    if attempt == 1 and state.get("phase", "seq") == "seq":
        ds_name = state.get("data_structure", "linked_list")
        user_prompt = _get_seq_prompt(ds_name)
    history.add_user(user_prompt)

    # ── Logging
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"
    timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"--- [SEQUENTIAL PHASE] Attempt {attempt} ---\n")
        f.write(f"Timestamp: {timestamp_now}\n")
        f.write(f"History turns sent to LLM: {len(history)}\n")
        f.flush()

        # ── LLM call (stateful — full history)
        try:
            llm = _llm()
            response = _call_llm_with_retry(llm, history.to_langchain_messages(), log_file)
            code = extract_text_from_response(response)
        except Exception as e:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write(f"ERROR: LLM API call failed after retries: {str(e)}\n")
                f.write(f"Error type: {type(e).__name__}\n")
                f.flush()
            raise

    # ── Store assistant response in history
    history.add_assistant(code)

    # ── Extract and validate C++ code
    cpp_code = _extract_cpp_code(code)
    fixed_code = _validate_and_fix_sequential_code(cpp_code)

    with open(log_file, "a", encoding="utf-8") as f:
        f.write("MODEL OUTPUT:\n")
        f.write(fixed_code + "\n")
        f.write("===\n\n")
        f.flush()

    return {
        "generated_code": fixed_code,
        "seq_attempt_count": attempt,
        "sequential_code": fixed_code,
        "conversation_history": history.to_dict_list(),
    }
