"""
C++ Zero-shot generation node.

This node generates lock-free concurrent data structures in C++17 using an LLM.
It loads the appropriate zero-shot prompt, calls the LLM, extracts C++ code,
and updates the state with the generated code.

Writes ONLY to the per-sample log file (state["log_file_path"]).
The continuous log is written by cpp_zero_shot_runner.py to avoid duplicate entries.
"""
from __future__ import annotations

import os
import re
from datetime import datetime
from pathlib import Path
from typing import Dict, Any

from langchain_openai import ChatOpenAI
from langchain_core.messages import SystemMessage, HumanMessage
from langsmith import traceable

from .cpp_state import CPPZeroShotState
from .cpp_prompts import ZERO_SHOT_PROMPTS, ZERO_SHOT_SYSTEM_PROMPT

_PROJECT_ROOT = Path(__file__).resolve().parents[1]


# ── Helpers ───────────────────────────────────────────────────────────────────

def _write_log(log_file: str, message: str) -> None:
    """Write to the per-sample log file."""
    try:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(message)
            f.flush()
    except Exception:
        pass


def _section(log_file: str, title: str) -> None:
    """Write a section header to the log."""
    bar = "=" * 80
    _write_log(log_file, f"\n{bar}\n{title}\n{bar}\n")


def _extract_cpp_code(text: str) -> str:
    """
    Extract C++ code from LLM response.
    
    Handles markdown code blocks (```cpp or ```c++) and raw C++ code.
    Returns the extracted code or the original text if no code block found.
    """
    lines = text.split("\n")
    cpp_lines = []
    in_block = False
    
    for line in lines:
        stripped = line.strip()
        
        # Start of code block
        if stripped.startswith("```cpp") or stripped.startswith("```c++"):
            in_block = True
            continue
        # End of code block
        elif stripped == "```" and in_block:
            break
        # Inside code block
        elif in_block:
            cpp_lines.append(line)
        # Raw C++ code (starts with #pragma, #include, class, etc.)
        elif not in_block and (
            stripped.startswith("#pragma")
            or stripped.startswith("#include")
            or stripped.startswith("class ")
            or "class ConcurrentDataStructure" in stripped
        ):
            cpp_lines.append(line)
            in_block = True
    
    return "\n".join(cpp_lines) if cpp_lines else text


def _validate_and_fix(cpp_code: str) -> str:
    """
    Validate and fix common issues in generated C++ code.
    
    - Ensures #pragma once is present
    - Ensures required includes are present
    - Ensures class name is ConcurrentDataStructure
    """
    lines = cpp_code.split("\n")
    fixed_lines = []
    has_pragma = False
    has_setadt_include = False
    has_atomic_include = False
    
    # Check what we have
    for line in lines:
        if "#pragma once" in line:
            has_pragma = True
        if '#include "../utils/SetADT.hpp"' in line or "#include <SetADT.hpp>" in line:
            has_setadt_include = True
        if "#include <atomic>" in line:
            has_atomic_include = True
    
    # Add missing headers at the top
    if not has_pragma:
        fixed_lines.append("#pragma once")
    
    # Add the rest of the code
    for line in lines:
        # Fix SetADT include path if needed
        if "#include <SetADT.hpp>" in line:
            line = line.replace("#include <SetADT.hpp>", '#include "../utils/SetADT.hpp"')
        fixed_lines.append(line)
    
    # Ensure SetADT include is present (add after #pragma once if missing)
    if not has_setadt_include:
        # Find position after #pragma once
        insert_pos = 0
        for i, line in enumerate(fixed_lines):
            if "#pragma once" in line:
                insert_pos = i + 1
                break
        fixed_lines.insert(insert_pos, '#include "../utils/SetADT.hpp"')
    
    # Ensure atomic include is present
    if not has_atomic_include:
        # Find position after includes
        insert_pos = 0
        for i, line in enumerate(fixed_lines):
            if "#include" in line:
                insert_pos = i + 1
        if insert_pos > 0:
            fixed_lines.insert(insert_pos, "#include <atomic>")
    
    cpp_code = "\n".join(fixed_lines)
    
    # Ensure class name is ConcurrentDataStructure
    # Match various class declaration patterns
    cpp_code = re.sub(
        r"class\s+\w+\s*:\s*public\s+SetADT",
        "class ConcurrentDataStructure : public SetADT",
        cpp_code,
        count=1
    )
    
    return cpp_code


def _is_empty_or_invalid(code: str) -> bool:
    """Check if the generated code is empty or invalid."""
    return (
        len(code.strip()) < 100
        or "ConcurrentDataStructure" not in code
        or "SetADT" not in code
    )


def _get_llm() -> ChatOpenAI:
    """Initialize and return the LLM client."""
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    if not api_key:
        raise ValueError("NVIDIA_NIM_API_KEY environment variable is not set")
    
    model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
    
    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key,
        model=model,
        temperature=0.5,
        max_tokens=8000,
    )


# ── LangGraph node ────────────────────────────────────────────────────────────

@traceable
def node_generate_cpp(state: CPPZeroShotState) -> Dict[str, Any]:
    """
    C++ zero-shot generation node.
    
    Loads the appropriate prompt for the data structure, calls the LLM,
    extracts C++ code from the response, validates and fixes common issues,
    and updates the state with the generated code.
    
    Args:
        state: CPPZeroShotState containing ds_name, sample_idx, and log paths
        
    Returns:
        Dictionary with updated state fields:
        - generated_code: The extracted and validated C++ code
        - stage_1_code: Snapshot of code after generation
        - error_message: Error message if generation failed (empty otherwise)
    """
    ds_name = state["ds_name"]
    sample_idx = state["sample_idx"]
    log_file = state["log_file_path"]
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    _section(log_file, f"[STAGE 1 — GENERATION] DS: {ds_name} | Sample: {sample_idx} | {timestamp}")
    
    # ── Load prompt ───────────────────────────────────────────────────────
    if ds_name not in ZERO_SHOT_PROMPTS:
        error_msg = f"Unknown data structure: {ds_name}"
        _write_log(log_file, f"[Generation] ERROR: {error_msg}\n")
        return {
            "generated_code": "",
            "stage_1_code": "",
            "error_message": error_msg,
        }
    
    user_prompt = ZERO_SHOT_PROMPTS[ds_name]
    _write_log(log_file, f"[Generation] Loaded prompt for {ds_name} ({len(user_prompt)} chars)\n")
    
    # ── Call LLM ──────────────────────────────────────────────────────────
    try:
        llm = _get_llm()
        model_name = os.environ.get("NVIDIA_NIM_MODEL", "unknown")
        _write_log(log_file, f"[Generation] Calling LLM: {model_name}\n")
        
        messages = [
            SystemMessage(content=ZERO_SHOT_SYSTEM_PROMPT),
            HumanMessage(content=user_prompt),
        ]
        
        response = llm.invoke(messages)
        raw_output: str = response.content if hasattr(response, "content") else str(response)
        
        _write_log(log_file, f"[Generation] LLM responded ({len(raw_output)} chars)\n")
        _write_log(log_file, f"[Generation] Raw output:\n{raw_output}\n{'='*80}\n\n")
        
    except Exception as exc:
        error_msg = f"LLM call failed: {exc}"
        _write_log(log_file, f"[Generation] ERROR: {error_msg}\n")
        return {
            "generated_code": "",
            "stage_1_code": "",
            "error_message": error_msg,
        }
    
    # ── Extract and validate ──────────────────────────────────────────────
    cpp_code = _extract_cpp_code(raw_output)
    fixed_code = _validate_and_fix(cpp_code)
    
    _write_log(log_file, f"[Generation] Extracted code ({len(cpp_code)} chars)\n")
    _write_log(log_file, f"[Generation] Fixed code ({len(fixed_code)} chars)\n")
    
    if _is_empty_or_invalid(fixed_code):
        error_msg = "LLM failed to generate valid ConcurrentDataStructure class"
        _write_log(log_file, f"[Generation] ERROR: {error_msg}\n")
        return {
            "generated_code": "",
            "stage_1_code": "",
            "error_message": error_msg,
        }
    
    _write_log(log_file, f"[Generation] Code validated successfully\n")
    _write_log(log_file, f"[Generation] Final code:\n{fixed_code}\n{'='*80}\n\n")
    
    # ── Update state ──────────────────────────────────────────────────────
    return {
        "generated_code": fixed_code,
        "stage_1_code": fixed_code,
        "error_message": "",
    }
