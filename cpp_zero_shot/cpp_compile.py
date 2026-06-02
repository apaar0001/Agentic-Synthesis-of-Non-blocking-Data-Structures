"""
C++ Zero-shot compilation node.

This node compiles generated C++ code to verify syntactic correctness.
It writes the code to ConcurrentDataStructure.hpp, calls the compiler,
and updates the state with compilation results.

Writes ONLY to the per-sample log file (state["log_file_path"]).
The continuous log is written by cpp_zero_shot_runner.py to avoid duplicate entries.
"""
from __future__ import annotations

from datetime import datetime
from pathlib import Path
from typing import Dict, Any

from langsmith import traceable

from .cpp_state import CPPZeroShotState
from cpp_test_integration.cpp_compiler import compile_cpp_code

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


# ── LangGraph node ────────────────────────────────────────────────────────────

@traceable
def node_compile_cpp(state: CPPZeroShotState) -> Dict[str, Any]:
    """
    C++ compilation node.
    
    Compiles the generated C++ code to verify syntactic correctness.
    Writes the code to ConcurrentDataStructure.hpp, creates a test_compile.cpp,
    runs clang++ compilation, and captures the results.
    
    Args:
        state: CPPZeroShotState containing generated_code and log paths
        
    Returns:
        Dictionary with updated state fields:
        - compilation_status: "pass" or "fail"
        - error_message: Compilation errors if failed (empty otherwise)
        - stage_2_code: Snapshot of code after compilation (same as generated_code)
        
    Requirements: 4.3.3, 4.3.4, 4.3.5
    """
    ds_name = state["ds_name"]
    sample_idx = state["sample_idx"]
    generated_code = state["generated_code"]
    log_file = state["log_file_path"]
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    _section(log_file, f"[STAGE 2 — COMPILATION] DS: {ds_name} | Sample: {sample_idx} | {timestamp}")
    
    # ── Check if we have code to compile ──────────────────────────────────
    if not generated_code or len(generated_code.strip()) < 50:
        error_msg = "No valid code to compile (generation may have failed)"
        _write_log(log_file, f"[Compilation] ERROR: {error_msg}\n")
        return {
            "compilation_status": "fail",
            "error_message": error_msg,
            "stage_2_code": generated_code,
        }
    
    _write_log(log_file, f"[Compilation] Compiling {len(generated_code)} chars of C++ code\n")
    
    # ── Create output directory ───────────────────────────────────────────
    # Use a temporary directory for compilation
    output_dir = _PROJECT_ROOT / "generated_code_cpp" / f"{ds_name}_sample_{sample_idx}"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    _write_log(log_file, f"[Compilation] Output directory: {output_dir}\n")
    
    # ── Call compiler ─────────────────────────────────────────────────────
    try:
        _write_log(log_file, f"[Compilation] Running clang++ -std=c++17 -O3 -pthread\n")
        
        result = compile_cpp_code(generated_code, output_dir)
        
        _write_log(log_file, f"[Compilation] Status: {result.status}\n")
        
        if result.stdout:
            _write_log(log_file, f"[Compilation] STDOUT:\n{result.stdout}\n")
        
        if result.stderr:
            _write_log(log_file, f"[Compilation] STDERR:\n{result.stderr}\n")
        
        # ── Handle compilation result ─────────────────────────────────────
        if result.status == "pass":
            _write_log(log_file, f"[Compilation] ✓ Compilation successful\n")
            _write_log(log_file, f"[Compilation] Binary: {output_dir}/test_compile\n")
            
            return {
                "compilation_status": "pass",
                "error_message": "",
                "stage_2_code": generated_code,
            }
        else:
            _write_log(log_file, f"[Compilation] ✗ Compilation failed\n")
            _write_log(log_file, f"[Compilation] Error: {result.error_message}\n")
            
            return {
                "compilation_status": "fail",
                "error_message": result.error_message,
                "stage_2_code": generated_code,
            }
            
    except Exception as exc:
        error_msg = f"Compilation exception: {exc}"
        _write_log(log_file, f"[Compilation] ERROR: {error_msg}\n")
        return {
            "compilation_status": "fail",
            "error_message": error_msg,
            "stage_2_code": generated_code,
        }
