"""
Judge node: verifies the generated code implements the requested data structure.

This is a zero-shot-only node that runs AFTER generation and BEFORE testing.
It is intentionally a hard gate with NO retry:
  VERDICT YES → proceed to test_conc
  VERDICT NO  → log_failure immediately

The judge uses a fresh LLM conversation (isolated from the generator's history)
so it cannot be biased by the generation dialogue.
"""
from __future__ import annotations

import os
import re
from pathlib import Path
from typing import Dict, Any

from langchain_openai import ChatOpenAI
from langchain_core.messages import SystemMessage, HumanMessage

from .state_zero_shot import ZeroShotState

_PROJECT_ROOT = Path(__file__).resolve().parents[1]
_CONTINUOUS_LOG = _PROJECT_ROOT / "continuous_logs_zero_shot.txt"


def _clog_live(message: str) -> None:
    """Write a brief live progress line to the continuous log (real-time)."""
    from datetime import datetime as _dt
    ts = _dt.now().strftime("%H:%M:%S")
    try:
        with open(_CONTINUOUS_LOG, "a", encoding="utf-8") as f:
            f.write(f"  [{ts}] {message}\n")
            f.flush()
    except Exception:
        pass


# ── Prompts ───────────────────────────────────────────────────────────────────

_JUDGE_SYSTEM = (
    "You are a code auditor specializing in Java data structures. "
    "Your sole task is to determine whether a given Java implementation "
    "matches the requested data structure type."
)

_JUDGE_USER_TEMPLATE = """\
You are given a Java concurrent data structure implementation.
Determine whether it correctly implements a {ds_label}.

Criteria:
- The Node class must contain the fields that are characteristic of a {ds_label} \
(e.g., for a linked list: next pointer; for a BST: left/right children; \
for a skip-list: array of next pointers; for a hash table: array/buckets; \
for a queue: head/tail with next; for a stack: top with next).
- The overall structure and shape of the data structure must match {ds_label}.
- Concurrent extras (locks, AtomicReference, mark fields, sentinel nodes, etc.) are allowed.
- Do NOT penalise for concurrency machinery — focus only on whether the underlying \
data structure matches.

Java Code:
{code}

Respond in EXACTLY this format (nothing else):
VERDICT: [YES/NO]
REASON: [One sentence. If YES, write "Correct data structure." Otherwise explain the mismatch \
without naming specific data structures — describe it in terms of node fields or shape.]"""


# ── DS label map ──────────────────────────────────────────────────────────────

_DS_LABELS: dict[str, str] = {
    "linked_list":         "singly-linked list",
    "binary_search_tree":  "binary search tree",
    "skiplist":            "skip list",
    "b_minus_tree":        "B-minus tree (B-tree)",
    "hash_table":          "hash table",
    "queue":               "queue",
    "stack":               "stack",
}


# ── Helpers ───────────────────────────────────────────────────────────────────

def _get_llm() -> ChatOpenAI:
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    model   = os.environ.get("NVIDIA_NIM_MODEL")
    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key,
        model=model,
        temperature=0.0,
        max_tokens=256,
    )


def _write_log(log_file: str, msg: str) -> None:
    """Write ONLY to the per-sample log file.
    The continuous log is handled exclusively by zero_shot_runner.py via merge."""
    try:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(msg)
            f.flush()
    except Exception:
        pass


# ── Node ──────────────────────────────────────────────────────────────────────

def node_judge_ds(state: ZeroShotState) -> Dict[str, Any]:
    """
    LLM judge: checks that the generated code is actually the requested DS.
    Hard gate — VERDICT NO goes straight to log_failure (no retry).

    State reads:  generated_code, ds_name, log_file_path
    State writes: ds_judge_status, error_message, failure_stage
    """
    code     = state.get("generated_code", "")
    ds_name  = state.get("ds_name", "")
    log_file = state.get("log_file_path") or "continuous_logs_zero_shot.txt"
    ds_label = _DS_LABELS.get(ds_name, ds_name.replace("_", " "))

    _write_log(log_file, "--- [DS JUDGE] ---\n")

    if not code:
        _write_log(log_file, "[DS Judge] No code to judge — marking fail.\n")
        return {
            "ds_judge_status":  "fail",
            "ds_judge_verdict": "NO",
            "ds_judge_reason":  "DS judge: no code was generated.",
            "error_message":    "DS judge: no code was generated.",
            "failure_stage":    "ds_judge",
        }

    messages = [
        SystemMessage(content=_JUDGE_SYSTEM),
        HumanMessage(content=_JUDGE_USER_TEMPLATE.format(
            ds_label=ds_label,
            code=code,
        )),
    ]

    try:
        llm      = _get_llm()
        _clog_live(f"DS Judge  | {ds_name} | Calling judge LLM…")
        response = llm.invoke(messages)
        text: str = response.content if hasattr(response, "content") else str(response)

        _write_log(log_file, f"[DS Judge] Response:\n{text}\n===\n\n")

        verdict_match = re.search(r"VERDICT:\s*(YES|NO)", text, re.IGNORECASE)
        verdict = verdict_match.group(1).upper() if verdict_match else "NO"

        reason = (
            text.split("REASON:")[1].strip()
            if "REASON:" in text
            else "Correct data structure." if verdict == "YES"
            else "Generated code does not implement the requested data structure."
        )

        if verdict == "YES":
            _write_log(log_file, "[DS Judge] PASSED — correct data structure.\n")
            _clog_live(f"DS Judge  | {ds_name} | VERDICT: YES ✓ → proceeding to test")
            return {
                "ds_judge_status":  "pass",
                "ds_judge_verdict": "YES",
                "ds_judge_reason":  reason,
            }

        # VERDICT NO — fail hard (no retry)
        _write_log(log_file, f"[DS Judge] FAILED — {reason}\n")
        _clog_live(f"DS Judge  | {ds_name} | VERDICT: NO ✗ — {reason[:80]}")
        return {
            "ds_judge_status":  "fail",
            "ds_judge_verdict": "NO",
            "ds_judge_reason":  reason,
            "error_message":    f"DS Judge Failed: {reason}",
            "failure_stage":    "ds_judge",
        }

    except Exception as exc:
        error_msg = f"DS judge error: {exc}"
        _write_log(log_file, f"{error_msg}\n")
        return {
            "ds_judge_status":  "fail",
            "ds_judge_verdict": "ERROR",
            "ds_judge_reason":  error_msg,
            "error_message":    error_msg,
            "failure_stage":    "ds_judge",
        }
