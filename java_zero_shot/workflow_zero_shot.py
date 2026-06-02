"""
LangGraph workflow for the zero-shot pipeline.

Graph topology:
  generate_zero_shot
    → judge_ds             [LLM judge: is the generated DS the one that was asked for?]
      → pass  → test_conc  [compile → consistency test → victim-inject → non-blocking test]
      → fail  → log_failure
    test_conc
      → pass  → log_success
      → fail  → log_failure   (no retry — pure zero-shot: one attempt only)

No retry or reprompt in the zero-shot pipeline.
Each sample is one clean attempt; failures are logged immediately.
"""
from __future__ import annotations

from langgraph.graph import StateGraph, START, END

from .state_zero_shot import ZeroShotState
from .generate_zero_shot import node_generate_zero_shot
from .judge_ds import node_judge_ds

# Reuse existing test / log nodes unchanged
from nodes.test_code import node_test_code_conc
from nodes.log_result_success import node_log_success
from nodes.log_result_failure import node_log_failure


# ── DS judge router ───────────────────────────────────────────────────────────

def _route_after_judge(state: ZeroShotState) -> str:
    """
    Route after the DS judge node.
    - pass → test_conc
    - fail → log_failure  (wrong DS is immediately disqualifying)
    """
    return "test_conc" if state.get("ds_judge_status") == "pass" else "log_failure"


# ── Test router ────────────────────────────────────────────────────────────────

def _route_after_test(state: ZeroShotState) -> str:
    """
    Pure zero-shot: no retries. Either pass → log_success, or fail → log_failure.
    """
    comp_ok   = state.get("compilation_status") == "pass"
    sanity_ok = state.get("sanity_status") == "pass"
    test_ok   = state.get("test_result") == "pass"
    lockf     = state.get("lock_freedom_status", "none")

    if comp_ok and sanity_ok and test_ok and lockf == "lock-free":
        return "log_success"
    return "log_failure"


# ── Graph builder ─────────────────────────────────────────────────────────────

def build_zero_shot_graph():
    """Build and compile the zero-shot LangGraph (no retries)."""
    graph = StateGraph(ZeroShotState)

    # Nodes
    graph.add_node("generate_zero_shot", node_generate_zero_shot)
    graph.add_node("judge_ds",           node_judge_ds)
    graph.add_node("test_conc",          node_test_code_conc)
    graph.add_node("log_success",        node_log_success)
    graph.add_node("log_failure",        node_log_failure)

    # Entry → generate → DS judge
    graph.set_entry_point("generate_zero_shot")
    graph.add_edge("generate_zero_shot", "judge_ds")

    # After DS judge: pass→test, fail→log_failure
    graph.add_conditional_edges(
        "judge_ds",
        _route_after_judge,
        {
            "test_conc":   "test_conc",
            "log_failure": "log_failure",
        },
    )

    # After test: success or immediate failure (no reprompt)
    graph.add_conditional_edges(
        "test_conc",
        _route_after_test,
        {
            "log_success": "log_success",
            "log_failure": "log_failure",
        },
    )

    # Terminals
    graph.add_edge("log_success", END)
    graph.add_edge("log_failure", END)

    return graph.compile()
