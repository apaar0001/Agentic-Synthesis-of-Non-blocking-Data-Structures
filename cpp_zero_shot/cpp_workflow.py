"""
C++ Zero-Shot LangGraph Workflow

This module defines the complete LangGraph workflow for the C++ zero-shot pipeline.
The workflow orchestrates 5 stages:
  1. Generation (LLM)
  2. Compilation (clang++)
  3. Consistency Test
  4. Non-Blocking Test (victim injection)
  5. Extended CodeBLEU Evaluation

The workflow includes conditional edges to handle compilation failures gracefully.

Requirements: Workflow orchestration (Task 14.1)
"""
from __future__ import annotations

from typing import Literal

from langgraph.graph import StateGraph, END
from langsmith import traceable

from .cpp_state import CPPZeroShotState
from .cpp_generate import node_generate_cpp
from .cpp_compile import node_compile_cpp
from .cpp_test import node_test_cpp
from .cpp_evaluate import node_evaluate_cpp


# ── Conditional edge functions ───────────────────────────────────────────────

def should_run_tests(state: CPPZeroShotState) -> Literal["test", "evaluate"]:
    """
    Decide whether to run tests or skip directly to evaluation.
    
    If compilation failed, skip tests and go directly to evaluation
    (which will assign zero scores).
    
    Args:
        state: Current pipeline state
        
    Returns:
        "test" if compilation passed, "evaluate" if compilation failed
    """
    compilation_status = state.get("compilation_status", "none")
    
    if compilation_status == "pass":
        return "test"
    else:
        # Compilation failed - skip tests and go to evaluation
        return "evaluate"


# ── Workflow builder ──────────────────────────────────────────────────────────

@traceable
def build_cpp_workflow() -> StateGraph:
    """
    Build the C++ zero-shot LangGraph workflow.
    
    Workflow structure:
    
        START
          ↓
        generate (node_generate_cpp)
          ↓
        compile (node_compile_cpp)
          ↓
        [conditional: compilation passed?]
          ↓                    ↓
        YES                   NO
          ↓                    ↓
        test                 evaluate
          ↓                    ↓
        evaluate              END
          ↓
        END
    
    Nodes:
    - generate: Call LLM to generate C++ code
    - compile: Compile code with clang++
    - test: Run ConsistencyTest and NonBlockingTest
    - evaluate: Compute Extended CodeBLEU scores
    
    Conditional edges:
    - After compile: If pass → test, if fail → evaluate
    
    Returns:
        Compiled StateGraph ready for execution
        
    Requirements: Task 14.1
    """
    # Create workflow graph
    workflow = StateGraph(CPPZeroShotState)
    
    # ── Add nodes ─────────────────────────────────────────────────────────
    workflow.add_node("generate", node_generate_cpp)
    workflow.add_node("compile", node_compile_cpp)
    workflow.add_node("test", node_test_cpp)
    workflow.add_node("evaluate", node_evaluate_cpp)
    
    # ── Set entry point ───────────────────────────────────────────────────
    workflow.set_entry_point("generate")
    
    # ── Add edges ─────────────────────────────────────────────────────────
    # generate → compile (always)
    workflow.add_edge("generate", "compile")
    
    # compile → [conditional]
    # If compilation passed → test
    # If compilation failed → evaluate (skip tests)
    workflow.add_conditional_edges(
        "compile",
        should_run_tests,
        {
            "test": "test",
            "evaluate": "evaluate",
        }
    )
    
    # test → evaluate (always)
    workflow.add_edge("test", "evaluate")
    
    # evaluate → END (always)
    workflow.add_edge("evaluate", END)
    
    # ── Compile graph ─────────────────────────────────────────────────────
    return workflow.compile()


# ── Convenience function ──────────────────────────────────────────────────────

def create_workflow():
    """
    Create and return a compiled C++ zero-shot workflow.
    
    This is a convenience function for external callers.
    
    Returns:
        Compiled LangGraph workflow
    """
    return build_cpp_workflow()
