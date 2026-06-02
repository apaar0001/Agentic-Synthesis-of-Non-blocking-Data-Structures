"""
Zero-shot generation node.

Writes ONLY to the per-sample log file (state["log_file_path"]).
The continuous log (continuous_logs_zero_shot.txt) is written exclusively
by zero_shot_runner.py to avoid duplicate entries.
"""
from __future__ import annotations

import os
import re
from datetime import datetime
from pathlib import Path
from typing import Dict, Any

from langchain_openai import ChatOpenAI
from langchain_core.messages import SystemMessage, HumanMessage, AIMessage
from langsmith import traceable

from .state_zero_shot import ZeroShotState
from .prompts import ZERO_SHOT_SYSTEM_PROMPT

_PROJECT_ROOT = Path(__file__).resolve().parents[1]
_CONTINUOUS_LOG = _PROJECT_ROOT / "continuous_logs_zero_shot.txt"


# ── Helpers ───────────────────────────────────────────────────────────────────

def _clog_live(message: str) -> None:
    """Write a brief live progress line to the continuous log (real-time)."""
    ts = datetime.now().strftime("%H:%M:%S")
    try:
        with open(_CONTINUOUS_LOG, "a", encoding="utf-8") as f:
            f.write(f"  [{ts}] {message}\n")
            f.flush()
    except Exception:
        pass


def _write_log(log_file: str, message: str) -> None:
    """Write ONLY to the per-sample log file. Continuous log is handled by the runner."""
    try:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(message)
            f.flush()
    except Exception:
        pass


def _section(log_file: str, title: str) -> None:
    bar = "=" * 80
    _write_log(log_file, f"\n{bar}\n{title}\n{bar}\n")


def _extract_java_code(text: str) -> str:
    lines = text.split("\n")
    java_lines, in_block = [], False
    for line in lines:
        if line.strip().startswith("```java"):
            in_block = True; continue
        elif line.strip() == "```" and in_block:
            break
        elif in_block:
            java_lines.append(line)
        elif not in_block and (
            line.strip().startswith("package ")
            or line.strip().startswith("import ")
            or line.strip().startswith("public class ")
            or line.strip().startswith("class ")
            or line.strip().startswith("interface ")
        ):
            java_lines.append(line); in_block = True
    return "\n".join(java_lines)


def _validate_and_fix(java_code: str) -> str:
    java_code = java_code.replace(".get(null)", ".getReference()")
    if "package " in java_code:
        java_code = re.sub(r"package\s+[^\n;]+;", "package com.example.Sets;", java_code, count=1)
    else:
        java_code = "package com.example.Sets;\n\n" + java_code
    if "import com.example.utils.SetADT;" not in java_code:
        java_code = java_code.replace(
            "package com.example.Sets;",
            "package com.example.Sets;\nimport com.example.utils.SetADT;", 1)
    java_code = re.sub(r"public\s+final\s+class\s+\w+", "public class ConcurrentDataStructure", java_code, count=1)
    java_code = re.sub(r"public\s+class\s+\w+", "public class ConcurrentDataStructure", java_code, count=1)
    return java_code


def _is_empty_or_invalid(code: str) -> bool:
    return len(code.strip()) < 100 or "ConcurrentDataStructure" not in code


def _get_llm() -> ChatOpenAI:
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    if not api_key:
        raise ValueError("NVIDIA_NIM_API_KEY environment variable is not set")
    model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key, model=model,
        temperature=0.5, max_tokens=8000,
    )


def _save_generated_code(code: str, state: ZeroShotState, log_file: str) -> Path:
    """Save code to generated_code_zero_shot/... and return the path."""
    model_name = os.environ.get("NVIDIA_NIM_MODEL", "unknown_model")
    model_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")
    ds = state.get("ds_name", "unknown_ds")
    p_idx = state.get("prompt_idx", 0)
    r_idx = state.get("run_idx", 0)
    out_dir = (
        _PROJECT_ROOT / "generated_code_zero_shot"
        / model_clean / ds / f"prompt_{p_idx}_sample_{r_idx}"
    )
    out_dir.mkdir(parents=True, exist_ok=True)
    out_path = out_dir / "ConcurrentDataStructure.java"
    out_path.write_text(code, encoding="utf-8")
    _write_log(log_file, f"[ZeroShot] Saved → {out_path}\n")
    return out_path


# ── LangGraph node ────────────────────────────────────────────────────────────

@traceable
def node_generate_zero_shot(state: ZeroShotState) -> Dict[str, Any]:
    """
    Zero-shot generation node.
    On attempt 1: sends the full zero-shot prompt.
    On retries:   uses state["current_prompt"] (set by reprompt node).

    Writes ONLY to per-sample log — NOT to continuous log.
    """
    attempt  = state.get("conc_attempt_count", 0) + 1
    log_file = state.get("log_file_path") or str(_PROJECT_ROOT / "continuous_logs_zero_shot.txt")
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")

    _section(log_file, f"[STAGE 1 — GENERATION] Attempt {attempt} | DS: {state.get('ds_name')} | {timestamp}")

    # ── Conversation history ──────────────────────────────────────────────
    conversation: list = list(state.get("conversation_history", []))
    if not conversation:
        conversation.append({"role": "system", "content": ZERO_SHOT_SYSTEM_PROMPT})

    user_msg = (
        state.get("original_prompt", state.get("current_prompt", ""))
        if attempt == 1
        else state.get("current_prompt", "")
    )
    conversation.append({"role": "user", "content": user_msg})
    _write_log(log_file, f"[Generation] History turns sent to LLM: {len(conversation)}\n")

    # If this is a retry, log what reprompt was used
    if attempt > 1:
        _write_log(log_file, f"[Generation] Retry prompt:\n{user_msg}\n---\n")

    # ── LLM call ─────────────────────────────────────────────────────────
    def _to_lc(msg: dict):
        if msg["role"] == "system":  return SystemMessage(content=msg["content"])
        if msg["role"] == "user":    return HumanMessage(content=msg["content"])
        return AIMessage(content=msg["content"])

    try:
        llm = _get_llm()
        _write_log(log_file, f"[Generation] Calling LLM: {os.environ.get('NVIDIA_NIM_MODEL')}\n")
        _clog_live(f"Stage 1 | {state.get('ds_name')} | Calling LLM {os.environ.get('NVIDIA_NIM_MODEL')}…")
        response = llm.invoke([_to_lc(m) for m in conversation])
        raw_output: str = response.content if hasattr(response, "content") else str(response)
        _clog_live(f"Stage 1 | {state.get('ds_name')} | LLM responded ({len(raw_output)} chars)")
    except Exception as exc:
        _write_log(log_file, f"[Generation] ERROR: LLM call failed: {exc}\n")
        _clog_live(f"Stage 1 | {state.get('ds_name')} | ERROR: LLM call failed: {exc}")
        raise

    conversation.append({"role": "assistant", "content": raw_output})

    # ── Extract & validate ────────────────────────────────────────────────
    java_code  = _extract_java_code(raw_output)
    fixed_code = _validate_and_fix(java_code)

    _write_log(log_file, f"[Generation] Model output ({len(raw_output)} chars):\n{raw_output}\n===\n\n")

    if _is_empty_or_invalid(fixed_code):
        _write_log(log_file, "[Generation] WARNING: Output empty or invalid — marking generation fail.\n")
        _clog_live(f"Stage 1 | {state.get('ds_name')} | WARNING: code empty/invalid — fail")
        return {
            "generated_code": "",
            "conc_attempt_count": attempt,
            "conversation_history": conversation,
            "structural_verify_status": "fail",
            "error_message": "LLM failed to generate a valid ConcurrentDataStructure class.",
            "failure_stage": "structural_verify",
        }

    _save_generated_code(fixed_code, state, log_file)
    _write_log(log_file, f"[Generation] Code extracted and validated ({len(fixed_code)} chars). OK.\n")
    _clog_live(f"Stage 1 | {state.get('ds_name')} | Code extracted OK ({len(fixed_code)} chars) → DS judge next")

    return {
        "generated_code":           fixed_code,
        "conc_attempt_count":       attempt,
        "conversation_history":     conversation,
        "sequential_code":          "",
        "structural_verify_status": "pass",
    }
