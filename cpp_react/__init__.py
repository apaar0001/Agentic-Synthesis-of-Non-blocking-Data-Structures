"""
cpp_react — C++ ReAct Translation Pipeline
=============================================

Two-phase agentic framework for C++ lock-free data structures:
  Phase 1: Generate sequential C++ implementation
  Phase 2: Translate sequential → lock-free concurrent with feedback loops

Mirrors the Java translation pipeline (runner.py + workflow.py).
"""

from .state import CppReactState
from .workflow import build_cpp_react_graph

__all__ = [
    "CppReactState",
    "build_cpp_react_graph",
]
