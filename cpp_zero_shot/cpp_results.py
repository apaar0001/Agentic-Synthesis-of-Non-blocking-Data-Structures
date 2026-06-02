"""
Results aggregation for C++ Zero-Shot Pipeline.

This module provides functions for saving sample results, aggregating statistics,
and writing benchmark summaries. It handles all output file generation including
code snapshots, logs, CSV results, and summary reports.

Requirements: AC 4.9.1-4.9.4 (Task 15)
"""
from __future__ import annotations

import csv
import json
from pathlib import Path
from typing import Dict, List, Any
from datetime import datetime

from .cpp_state import CPPZeroShotState


# ── Sample Results ────────────────────────────────────────────────────────────

def save_sample_results(state: CPPZeroShotState, output_dir: Path) -> None:
    """
    Save all results for a single sample to the output directory.
    
    Creates the following structure:
    
    output_dir/
    ├── ConcurrentDataStructure.hpp  # Final generated code
    ├── sample_log.txt               # Per-sample execution log
    ├── codebleu_report.json         # All CodeBLEU layer scores
    └── stages/                      # Code snapshots at each stage
        ├── stage_1_generation.hpp
        ├── stage_2_compilation.hpp
        ├── stage_3_consistency.hpp
        ├── stage_4_victim.hpp
        └── stage_5_final.hpp
    
    Args:
        state: Pipeline state containing all results
        output_dir: Directory to save results (will be created if needed)
        
    Requirements: AC 4.9.4 (Task 15.1)
    """
    # Create output directory
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Save final generated code
    final_code_path = output_dir / "ConcurrentDataStructure.hpp"
    final_code_path.write_text(state["final_code"])
    
    # Save CodeBLEU report
    codebleu_report = {
        "ds_name": state["ds_name"],
        "sample_idx": state["sample_idx"],
        "model_name": state["model_name"],
        "timestamp": datetime.now().isoformat(),
        "test_results": {
            "compilation_status": state["compilation_status"],
            "consistency_status": state["consistency_status"],
            "lock_freedom_status": state["lock_freedom_status"],
            "lock_syntax_status": state["lock_syntax_status"],
        },
        "codebleu_scores": {
            "layer_a_consistency": state["layer_a_score"],
            "layer_b_nonblocking": state["layer_b_score"],
            "layer_c_codebleu": state["layer_c_score"],
            "layer_d1_annotation": state["layer_d1_score"],
            "layer_d2_concurrency": state["layer_d2_score"],
            "layer_d3_llm_judge": state["layer_d3_score"],
            "layer_d4_structural": state["layer_d4_score"],
            "combined_score": state["combined_score"],
        },
        "error_message": state["error_message"],
    }
    
    codebleu_path = output_dir / "codebleu_report.json"
    codebleu_path.write_text(json.dumps(codebleu_report, indent=2))
    
    # Save stage snapshots
    stages_dir = output_dir / "stages"
    stages_dir.mkdir(exist_ok=True)
    
    stage_files = [
        ("stage_1_generation.hpp", state["stage_1_code"]),
        ("stage_2_compilation.hpp", state["stage_2_code"]),
        ("stage_3_consistency.hpp", state["stage_3_code"]),
        ("stage_4_victim.hpp", state["stage_4_code"]),
        ("stage_5_final.hpp", state["stage_5_code"]),
    ]
    
    for filename, code in stage_files:
        stage_path = stages_dir / filename
        stage_path.write_text(code if code else "")
    
    # Note: sample_log.txt is written separately by the logger module
    # We don't copy it here to avoid duplication


# ── CSV Writer ────────────────────────────────────────────────────────────────

def append_to_csv(state: CPPZeroShotState, csv_path: Path) -> None:
    """
    Append a single sample's results to the CSV file.
    
    Creates the CSV file with headers if it doesn't exist.
    Appends a row with all stage results and CodeBLEU scores.
    
    CSV columns:
    - ds_name, sample_idx, model_name
    - compilation_status, consistency_status, lock_freedom_status, lock_syntax_status
    - layer_a_score, layer_b_score, layer_c_score
    - layer_d1_score, layer_d2_score, layer_d3_score, layer_d4_score
    - combined_score
    - error_message (truncated to 200 chars)
    
    Args:
        state: Pipeline state containing results
        csv_path: Path to CSV file (will be created if needed)
        
    Requirements: AC 4.9.1 (Task 15.2)
    """
    # Define CSV headers
    headers = [
        "ds_name",
        "sample_idx",
        "model_name",
        "compilation_status",
        "consistency_status",
        "lock_freedom_status",
        "lock_syntax_status",
        "layer_a_score",
        "layer_b_score",
        "layer_c_score",
        "layer_d1_score",
        "layer_d2_score",
        "layer_d3_score",
        "layer_d4_score",
        "combined_score",
        "error_message",
    ]
    
    # Check if file exists
    file_exists = csv_path.exists()
    
    # Open file in append mode
    with open(csv_path, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        
        # Write header if file is new
        if not file_exists:
            writer.writeheader()
        
        # Prepare row data
        row = {
            "ds_name": state["ds_name"],
            "sample_idx": state["sample_idx"],
            "model_name": state["model_name"],
            "compilation_status": state["compilation_status"],
            "consistency_status": state["consistency_status"],
            "lock_freedom_status": state["lock_freedom_status"],
            "lock_syntax_status": state["lock_syntax_status"],
            "layer_a_score": f"{state['layer_a_score']:.4f}",
            "layer_b_score": f"{state['layer_b_score']:.4f}",
            "layer_c_score": f"{state['layer_c_score']:.4f}",
            "layer_d1_score": f"{state['layer_d1_score']:.4f}",
            "layer_d2_score": f"{state['layer_d2_score']:.4f}",
            "layer_d3_score": f"{state['layer_d3_score']:.4f}",
            "layer_d4_score": f"{state['layer_d4_score']:.4f}",
            "combined_score": f"{state['combined_score']:.4f}",
            "error_message": state["error_message"][:200],  # Truncate long errors
        }
        
        # Write row
        writer.writerow(row)


# ── Stats Aggregation ─────────────────────────────────────────────────────────

def compute_stats(results: List[CPPZeroShotState]) -> Dict[str, Any]:
    """
    Compute aggregate statistics over a list of sample results.
    
    Computes:
    - Total samples
    - Pass rates (compilation, consistency, lock-freedom)
    - Average scores (all layers + combined)
    - Score distributions (min, max, median)
    
    Args:
        results: List of pipeline states from completed samples
        
    Returns:
        Dictionary containing all computed statistics
        
    Requirements: AC 4.9.2 (Task 15.3)
    """
    if not results:
        return {
            "total_samples": 0,
            "error": "No results to aggregate",
        }
    
    n = len(results)
    
    # ── Pass rates ────────────────────────────────────────────────────────
    compilation_pass = sum(1 for r in results if r["compilation_status"] == "pass")
    consistency_pass = sum(1 for r in results if r["consistency_status"] == "pass")
    lock_free = sum(1 for r in results if r["lock_freedom_status"] == "lock-free")
    
    # ── Score averages ────────────────────────────────────────────────────
    avg_layer_a = sum(r["layer_a_score"] for r in results) / n
    avg_layer_b = sum(r["layer_b_score"] for r in results) / n
    avg_layer_c = sum(r["layer_c_score"] for r in results) / n
    avg_layer_d1 = sum(r["layer_d1_score"] for r in results) / n
    avg_layer_d2 = sum(r["layer_d2_score"] for r in results) / n
    avg_layer_d3 = sum(r["layer_d3_score"] for r in results) / n
    avg_layer_d4 = sum(r["layer_d4_score"] for r in results) / n
    avg_combined = sum(r["combined_score"] for r in results) / n
    
    # ── Score distributions ───────────────────────────────────────────────
    combined_scores = sorted([r["combined_score"] for r in results])
    min_combined = combined_scores[0]
    max_combined = combined_scores[-1]
    median_combined = combined_scores[n // 2]
    
    # ── Build stats dictionary ────────────────────────────────────────────
    stats = {
        "total_samples": n,
        "pass_rates": {
            "compilation": {
                "count": compilation_pass,
                "rate": compilation_pass / n,
            },
            "consistency": {
                "count": consistency_pass,
                "rate": consistency_pass / n,
            },
            "lock_freedom": {
                "count": lock_free,
                "rate": lock_free / n,
            },
        },
        "average_scores": {
            "layer_a_consistency": avg_layer_a,
            "layer_b_nonblocking": avg_layer_b,
            "layer_c_codebleu": avg_layer_c,
            "layer_d1_annotation": avg_layer_d1,
            "layer_d2_concurrency": avg_layer_d2,
            "layer_d3_llm_judge": avg_layer_d3,
            "layer_d4_structural": avg_layer_d4,
            "combined": avg_combined,
        },
        "score_distribution": {
            "min": min_combined,
            "max": max_combined,
            "median": median_combined,
        },
    }
    
    return stats


# ── Stats Writer ──────────────────────────────────────────────────────────────

def write_stats(stats: Dict[str, Any], output_path: Path) -> None:
    """
    Write human-readable statistics file.
    
    Creates a formatted text file with all aggregate statistics including
    pass rates, average scores, and score distributions.
    
    Args:
        stats: Statistics dictionary from compute_stats()
        output_path: Path to write stats file
        
    Requirements: AC 4.9.2 (Task 15.4)
    """
    lines = []
    
    # ── Header ────────────────────────────────────────────────────────────
    lines.append("=" * 80)
    lines.append("C++ ZERO-SHOT PIPELINE STATISTICS")
    lines.append("=" * 80)
    lines.append("")
    
    # Check for error
    if "error" in stats:
        lines.append(f"ERROR: {stats['error']}")
        output_path.write_text("\n".join(lines))
        return
    
    # ── Summary ───────────────────────────────────────────────────────────
    lines.append(f"Total Samples: {stats['total_samples']}")
    lines.append("")
    
    # ── Pass Rates ────────────────────────────────────────────────────────
    lines.append("PASS RATES")
    lines.append("-" * 80)
    
    pr = stats["pass_rates"]
    lines.append(f"  Compilation:  {pr['compilation']['count']:3d} / {stats['total_samples']:3d}  ({pr['compilation']['rate']:6.1%})")
    lines.append(f"  Consistency:  {pr['consistency']['count']:3d} / {stats['total_samples']:3d}  ({pr['consistency']['rate']:6.1%})")
    lines.append(f"  Lock-Freedom: {pr['lock_freedom']['count']:3d} / {stats['total_samples']:3d}  ({pr['lock_freedom']['rate']:6.1%})")
    lines.append("")
    
    # ── Average Scores ────────────────────────────────────────────────────
    lines.append("AVERAGE SCORES")
    lines.append("-" * 80)
    
    avg = stats["average_scores"]
    lines.append(f"  Layer A (Consistency):  {avg['layer_a_consistency']:.4f}")
    lines.append(f"  Layer B (Non-Blocking): {avg['layer_b_nonblocking']:.4f}")
    lines.append(f"  Layer C (CodeBLEU):     {avg['layer_c_codebleu']:.4f}")
    lines.append(f"  Layer D1 (Annotation):  {avg['layer_d1_annotation']:.4f}")
    lines.append(f"  Layer D2 (Concurrency): {avg['layer_d2_concurrency']:.4f}")
    lines.append(f"  Layer D3 (LLM Judge):   {avg['layer_d3_llm_judge']:.4f}")
    lines.append(f"  Layer D4 (Structural):  {avg['layer_d4_structural']:.4f}")
    lines.append(f"  Combined Score:         {avg['combined']:.4f}")
    lines.append("")
    
    # ── Score Distribution ────────────────────────────────────────────────
    lines.append("SCORE DISTRIBUTION (Combined)")
    lines.append("-" * 80)
    
    dist = stats["score_distribution"]
    lines.append(f"  Min:    {dist['min']:.4f}")
    lines.append(f"  Median: {dist['median']:.4f}")
    lines.append(f"  Max:    {dist['max']:.4f}")
    lines.append("")
    
    # ── Footer ────────────────────────────────────────────────────────────
    lines.append("=" * 80)
    
    # Write to file
    output_path.write_text("\n".join(lines))


# ── Benchmark Summary ─────────────────────────────────────────────────────────

def write_benchmark_summary(
    model: str,
    ds: str,
    stats: Dict[str, Any],
    output_dir: Path,
) -> None:
    """
    Write benchmark summary files (JSON and CSV).
    
    Appends results to benchmark_summary.json and benchmark_summary.csv
    in the output directory. Creates files if they don't exist.
    
    Args:
        model: Model name (e.g., 'gpt-4')
        ds: Data structure name (e.g., 'linked_list')
        stats: Statistics dictionary from compute_stats()
        output_dir: Directory to write summary files
        
    Requirements: AC 4.9.3 (Task 15.5)
    """
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # ── Prepare summary entry ─────────────────────────────────────────────
    summary_entry = {
        "model": model,
        "data_structure": ds,
        "timestamp": datetime.now().isoformat(),
        "total_samples": stats.get("total_samples", 0),
        "pass_rates": stats.get("pass_rates", {}),
        "average_scores": stats.get("average_scores", {}),
        "score_distribution": stats.get("score_distribution", {}),
    }
    
    # ── Write JSON summary ────────────────────────────────────────────────
    json_path = output_dir / "benchmark_summary.json"
    
    # Load existing summaries
    if json_path.exists():
        with open(json_path, "r") as f:
            summaries = json.load(f)
    else:
        summaries = []
    
    # Append new entry
    summaries.append(summary_entry)
    
    # Write back
    with open(json_path, "w") as f:
        json.dump(summaries, f, indent=2)
    
    # ── Write CSV summary ─────────────────────────────────────────────────
    csv_path = output_dir / "benchmark_summary.csv"
    
    # Define CSV headers
    headers = [
        "model",
        "data_structure",
        "timestamp",
        "total_samples",
        "compilation_pass_rate",
        "consistency_pass_rate",
        "lock_freedom_pass_rate",
        "avg_combined_score",
        "min_combined_score",
        "max_combined_score",
        "median_combined_score",
    ]
    
    # Check if file exists
    file_exists = csv_path.exists()
    
    # Open file in append mode
    with open(csv_path, "a", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=headers)
        
        # Write header if file is new
        if not file_exists:
            writer.writeheader()
        
        # Prepare row data
        pr = stats.get("pass_rates", {})
        avg = stats.get("average_scores", {})
        dist = stats.get("score_distribution", {})
        
        row = {
            "model": model,
            "data_structure": ds,
            "timestamp": summary_entry["timestamp"],
            "total_samples": stats.get("total_samples", 0),
            "compilation_pass_rate": f"{pr.get('compilation', {}).get('rate', 0):.4f}",
            "consistency_pass_rate": f"{pr.get('consistency', {}).get('rate', 0):.4f}",
            "lock_freedom_pass_rate": f"{pr.get('lock_freedom', {}).get('rate', 0):.4f}",
            "avg_combined_score": f"{avg.get('combined', 0):.4f}",
            "min_combined_score": f"{dist.get('min', 0):.4f}",
            "max_combined_score": f"{dist.get('max', 0):.4f}",
            "median_combined_score": f"{dist.get('median', 0):.4f}",
        }
        
        # Write row
        writer.writerow(row)
