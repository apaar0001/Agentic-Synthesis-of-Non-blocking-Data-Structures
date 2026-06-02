"""
C++ Zero-shot evaluation node.

This node evaluates generated C++ code using the Extended CodeBLEU system.
It calls cpp_extended_codebleu.evaluate_cpp_file() to compute all layer scores
and updates the state with the results.

Writes ONLY to the per-sample log file (state["log_file_path"]).
The continuous log is written by cpp_zero_shot_runner.py to avoid duplicate entries.

Requirements: 4.7.1-4.7.9
"""
from __future__ import annotations

from datetime import datetime
from pathlib import Path
from typing import Dict, Any

from langsmith import traceable

from .cpp_state import CPPZeroShotState
from cpp_codebleu.cpp_extended_codebleu import evaluate_cpp_file

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
def node_evaluate_cpp(state: CPPZeroShotState) -> Dict[str, Any]:
    """
    C++ evaluation node using Extended CodeBLEU.
    
    Calls cpp_extended_codebleu.evaluate_cpp_file() to compute all layer scores:
    - Layer A: Consistency test (multiplier)
    - Layer B: Non-blocking test (multiplier)
    - Layer C: Multi-reference CodeBLEU
    - Layer D1: Annotation score
    - Layer D2: Concurrency primitives
    - Layer D3: LLM judge (optional)
    - Layer D4: Structural patterns
    
    Updates state with all scores and sets stage_5_code (final).
    
    Args:
        state: CPPZeroShotState containing generated code and test results
        
    Returns:
        Dictionary with updated state fields:
        - layer_a_score: Consistency test multiplier
        - layer_b_score: Non-blocking test multiplier
        - layer_c_score: Multi-ref CodeBLEU score
        - layer_d1_score: Annotation score
        - layer_d2_score: Concurrency primitives score
        - layer_d3_score: LLM judge score
        - layer_d4_score: Structural patterns score
        - combined_score: Final weighted score
        - stage_5_code: Final code snapshot
        - final_code: Same as stage_5_code
        
    Requirements: 4.7.1-4.7.9
    """
    ds_name = state["ds_name"]
    sample_idx = state["sample_idx"]
    log_file = state["log_file_path"]
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    _section(log_file, f"[STAGE 5 — EVALUATION] DS: {ds_name} | Sample: {sample_idx} | {timestamp}")
    
    # ── Prepare test results dictionary ───────────────────────────────────
    test_results = {
        "consistency_status": state.get("consistency_status", "none"),
        "lock_freedom_status": state.get("lock_freedom_status", "none"),
        "lock_syntax_status": state.get("lock_syntax_status", "unknown"),
    }
    
    _write_log(log_file, f"[Evaluation] Test results:\n")
    _write_log(log_file, f"  - Consistency: {test_results['consistency_status']}\n")
    _write_log(log_file, f"  - Lock-freedom (semantic): {test_results['lock_freedom_status']}\n")
    _write_log(log_file, f"  - Lock-freedom (syntactic): {test_results['lock_syntax_status']}\n")
    
    # ── Write code to temporary file for evaluation ───────────────────────
    # Use the generated_code (original, not victim-injected)
    code_to_evaluate = state.get("generated_code", "")
    
    if not code_to_evaluate or len(code_to_evaluate.strip()) < 50:
        error_msg = "No valid code to evaluate"
        _write_log(log_file, f"[Evaluation] ERROR: {error_msg}\n")
        return {
            "layer_a_score": 0.0,
            "layer_b_score": 0.0,
            "layer_c_score": 0.0,
            "layer_d1_score": 0.0,
            "layer_d2_score": 0.0,
            "layer_d3_score": 0.5,
            "layer_d4_score": 0.0,
            "combined_score": 0.0,
            "stage_5_code": code_to_evaluate,
            "final_code": code_to_evaluate,
        }
    
    # Create temporary file for evaluation
    output_dir = _PROJECT_ROOT / "generated_code_cpp" / f"{ds_name}_sample_{sample_idx}"
    output_dir.mkdir(parents=True, exist_ok=True)
    hpp_path = output_dir / "ConcurrentDataStructure.hpp"
    
    try:
        hpp_path.write_text(code_to_evaluate, encoding="utf-8")
        _write_log(log_file, f"[Evaluation] Code written to: {hpp_path}\n")
    except Exception as exc:
        error_msg = f"Failed to write code for evaluation: {exc}"
        _write_log(log_file, f"[Evaluation] ERROR: {error_msg}\n")
        return {
            "layer_a_score": 0.0,
            "layer_b_score": 0.0,
            "layer_c_score": 0.0,
            "layer_d1_score": 0.0,
            "layer_d2_score": 0.0,
            "layer_d3_score": 0.5,
            "layer_d4_score": 0.0,
            "combined_score": 0.0,
            "stage_5_code": code_to_evaluate,
            "final_code": code_to_evaluate,
        }
    
    # ── Call Extended CodeBLEU ────────────────────────────────────────────
    _write_log(log_file, f"[Evaluation] Running Extended CodeBLEU...\n")
    
    try:
        # Call evaluate_cpp_file with test results
        result = evaluate_cpp_file(
            hpp_path=hpp_path,
            ds_name=ds_name,
            test_results=test_results,
            use_llm_judge=True,  # Enable LLM judge by default
            approach="zero_shot",
        )
        
        _write_log(log_file, f"[Evaluation] Extended CodeBLEU completed\n")
        
        # ── Extract scores ────────────────────────────────────────────────
        layer_a = result.get("consistency_score", 0.0)
        layer_b = result.get("nonblocking_score", 0.0)
        layer_c = result.get("multi_ref_cb_score", 0.0)
        layer_d1 = result.get("annotation_score", 0.0)
        layer_d2 = result.get("concurrency_score", 0.0)
        layer_d3 = result.get("llm_judge_score", 0.5)
        layer_d4 = result.get("structural_patterns_score", 0.0)
        combined = result.get("combined_score", 0.0)
        
        # ── Log all scores ────────────────────────────────────────────────
        _write_log(log_file, f"\n[Evaluation] SCORES:\n")
        _write_log(log_file, f"{'='*80}\n")
        _write_log(log_file, f"  Layer A (Consistency):       {layer_a:.4f} - {result.get('consistency_detail', '')}\n")
        _write_log(log_file, f"  Layer B (Non-Blocking):      {layer_b:.4f} - {result.get('nonblocking_detail', '')}\n")
        _write_log(log_file, f"  Layer C (Multi-Ref CB):      {layer_c:.4f} - {result.get('multi_ref_cb_detail', '')}\n")
        _write_log(log_file, f"  Layer D1 (Annotation):       {layer_d1:.4f} - {result.get('annotation_detail', '')}\n")
        _write_log(log_file, f"  Layer D2 (Concurrency):      {layer_d2:.4f} - {result.get('concurrency_detail', '')}\n")
        _write_log(log_file, f"  Layer D3 (LLM Judge):        {layer_d3:.4f} - {result.get('llm_judge_verdict', 'SKIP')}\n")
        if result.get('llm_judge_reason'):
            _write_log(log_file, f"    Reason: {result.get('llm_judge_reason', '')}\n")
        _write_log(log_file, f"  Layer D4 (Structural):       {layer_d4:.4f} - {result.get('structural_patterns_detail', '')}\n")
        _write_log(log_file, f"{'='*80}\n")
        _write_log(log_file, f"  COMBINED SCORE:              {combined:.4f}\n")
        _write_log(log_file, f"  Is Correct DS:               {result.get('is_correct_ds', False)}\n")
        _write_log(log_file, f"{'='*80}\n")
        
        if result.get("error"):
            _write_log(log_file, f"\n[Evaluation] Warning: {result['error']}\n")
        
        # ── Update state ──────────────────────────────────────────────────
        return {
            "layer_a_score": layer_a,
            "layer_b_score": layer_b,
            "layer_c_score": layer_c,
            "layer_d1_score": layer_d1,
            "layer_d2_score": layer_d2,
            "layer_d3_score": layer_d3,
            "layer_d4_score": layer_d4,
            "combined_score": combined,
            "stage_5_code": code_to_evaluate,
            "final_code": code_to_evaluate,
        }
        
    except Exception as exc:
        error_msg = f"Extended CodeBLEU failed: {exc}"
        _write_log(log_file, f"[Evaluation] ERROR: {error_msg}\n")
        
        # Return zero scores on error
        return {
            "layer_a_score": 0.0,
            "layer_b_score": 0.0,
            "layer_c_score": 0.0,
            "layer_d1_score": 0.0,
            "layer_d2_score": 0.0,
            "layer_d3_score": 0.5,
            "layer_d4_score": 0.0,
            "combined_score": 0.0,
            "stage_5_code": code_to_evaluate,
            "final_code": code_to_evaluate,
        }
