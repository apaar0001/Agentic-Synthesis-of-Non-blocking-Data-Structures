#!/usr/bin/env python3
"""
C++ Zero-Shot Pipeline Runner

Main orchestrator for the C++ zero-shot pipeline. This script:
1. Parses command-line arguments (--ds, --num_runs, --model)
2. Initializes directories and logging
3. Builds the LangGraph workflow
4. Iterates over data structures and samples
5. Invokes workflow for each sample
6. Saves results (code, logs, CSV, stats)
7. Handles errors gracefully
8. Reports progress to console

Requirements: AC 4.8.1-4.8.5 (Task 17)

Usage:
    python cpp_zero_shot_runner.py --ds linked_list --num_runs 10
    python cpp_zero_shot_runner.py --ds skiplist bst --num_runs 5 --model gpt-4
    python cpp_zero_shot_runner.py --num_runs 3  # All data structures
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import List
from datetime import datetime
import os
from dotenv import load_dotenv

# Import pipeline modules
from cpp_zero_shot.cpp_workflow import create_workflow
from cpp_zero_shot.cpp_state import CPPZeroShotState
from cpp_zero_shot.cpp_prompts import ZERO_SHOT_PROMPTS, ZERO_SHOT_SYSTEM_PROMPT
from cpp_zero_shot import cpp_logger
from cpp_zero_shot.cpp_results import (
    save_sample_results,
    append_to_csv,
    compute_stats,
    write_stats,
    write_benchmark_summary,
)


# ── Constants ─────────────────────────────────────────────────────────────────

# Supported data structures
SUPPORTED_DS = ["linked_list", "skiplist", "bst", "hash_table"]

# Default model
DEFAULT_MODEL = "nvidia/llama-3.1-nemotron-ultra-253b-v1"

# Directory structure
BASE_DIR = Path(".")
LOGS_DIR = BASE_DIR / "logs_cpp"
GENERATED_CODE_DIR = BASE_DIR / "generated_code_cpp"
RESULTS_DIR = BASE_DIR / "results_cpp"


# ── Argument Parsing ──────────────────────────────────────────────────────────

def parse_args() -> argparse.Namespace:
    """
    Parse command-line arguments.
    
    Returns:
        Parsed arguments namespace
        
    Requirements: AC 4.8.1 (Task 17.1)
    """
    parser = argparse.ArgumentParser(
        description="C++ Zero-Shot Pipeline Runner",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python cpp_zero_shot_runner.py --ds linked_list --num_runs 10
  python cpp_zero_shot_runner.py --ds skiplist bst --num_runs 5 --model gpt-4
  python cpp_zero_shot_runner.py --num_runs 3  # All data structures
        """,
    )
    
    parser.add_argument(
        "--ds",
        nargs="+",
        choices=SUPPORTED_DS,
        default=SUPPORTED_DS,
        help=f"Data structures to generate (default: all). Choices: {', '.join(SUPPORTED_DS)}",
    )
    
    parser.add_argument(
        "--num_runs",
        type=int,
        default=1,
        help="Number of samples to generate per data structure (default: 1)",
    )
    
    parser.add_argument(
        "--model",
        type=str,
        default=DEFAULT_MODEL,
        help=f"LLM model to use (default: {DEFAULT_MODEL})",
    )
    
    parser.add_argument(
        "--skip_llm_judge",
        action="store_true",
        help="Skip Layer D3 (LLM Judge) evaluation to save API calls",
    )
    
    return parser.parse_args()


# ── Directory Initialization ──────────────────────────────────────────────────

def init_directories() -> None:
    """
    Initialize output directories.
    
    Creates:
    - logs_cpp/
    - generated_code_cpp/
    - results_cpp/
    
    Requirements: AC 4.8.1 (Task 17.1)
    """
    LOGS_DIR.mkdir(exist_ok=True)
    GENERATED_CODE_DIR.mkdir(exist_ok=True)
    RESULTS_DIR.mkdir(exist_ok=True)


# ── State Initialization ──────────────────────────────────────────────────────

def create_initial_state(
    ds_name: str,
    sample_idx: int,
    model_name: str,
    continuous_log_path: Path,
) -> CPPZeroShotState:
    """
    Create initial state for a sample.
    
    Args:
        ds_name: Data structure name
        sample_idx: Sample index (1-indexed)
        model_name: LLM model name
        continuous_log_path: Path to continuous log file
        
    Returns:
        Initialized CPPZeroShotState
        
    Requirements: Task 17.2
    """
    # Get prompt for this data structure
    prompt = ZERO_SHOT_PROMPTS.get(ds_name, "")
    
    # Create per-sample log path
    log_file_path = LOGS_DIR / f"{ds_name}_sample_{sample_idx}.txt"
    
    # Initialize state with all required fields
    state: CPPZeroShotState = {
        # Identity
        "ds_name": ds_name,
        "sample_idx": sample_idx,
        "model_name": model_name,
        
        # Prompts
        "original_prompt": prompt,
        "current_prompt": prompt,
        
        # Generated code
        "generated_code": "",
        "final_code": "",
        
        # Stage snapshots
        "stage_1_code": "",
        "stage_2_code": "",
        "stage_3_code": "",
        "stage_4_code": "",
        "stage_5_code": "",
        
        # Test results
        "compilation_status": "none",
        "consistency_status": "none",
        "lock_freedom_status": "none",
        "lock_syntax_status": "unknown",
        
        # Errors
        "error_message": "",
        
        # CodeBLEU scores
        "layer_a_score": 0.0,
        "layer_b_score": 0.0,
        "layer_c_score": 0.0,
        "layer_d1_score": 0.0,
        "layer_d2_score": 0.0,
        "layer_d3_score": 0.0,
        "layer_d4_score": 0.0,
        "combined_score": 0.0,
        
        # Logging
        "log_file_path": str(log_file_path),
        "continuous_log_path": str(continuous_log_path),
    }
    
    return state


# ── Main Loop ─────────────────────────────────────────────────────────────────

def run_pipeline(args: argparse.Namespace) -> None:
    """
    Run the complete pipeline for all data structures and samples.
    
    Args:
        args: Parsed command-line arguments
        
    Requirements: AC 4.8.2, 4.8.3, 4.8.4, 4.8.5 (Task 17.2)
    """
    # Initialize directories
    init_directories()
    
    # Initialize continuous log
    continuous_log_path = BASE_DIR / "continuous_logs_cpp.txt"
    cpp_logger.init_continuous_log(continuous_log_path)
    
    # Log pipeline start
    cpp_logger.log_section("C++ ZERO-SHOT PIPELINE START")
    cpp_logger.log_info(f"Model: {args.model}")
    cpp_logger.log_info(f"Data Structures: {', '.join(args.ds)}")
    cpp_logger.log_info(f"Samples per DS: {args.num_runs}")
    
    # Build workflow
    print("Building LangGraph workflow...")
    workflow = create_workflow()
    print("Workflow built successfully.\n")
    
    # Track overall statistics
    total_samples = 0
    total_errors = 0
    
    # Iterate over data structures
    for ds_name in args.ds:
        print(f"\n{'=' * 80}")
        print(f"DATA STRUCTURE: {ds_name.upper()}")
        print(f"{'=' * 80}\n")
        
        cpp_logger.log_section(f"{ds_name.upper()} SAMPLES")
        
        # Track results for this DS
        ds_results: List[CPPZeroShotState] = []
        
        # Iterate over samples
        for sample_idx in range(1, args.num_runs + 1):
            total_samples += 1
            
            print(f"\n{'-' * 80}")
            print(f"Sample {sample_idx}/{args.num_runs} - {ds_name}")
            print(f"{'-' * 80}\n")
            
            cpp_logger.log_sample_start(ds_name, sample_idx)
            
            try:
                # Create initial state
                state = create_initial_state(
                    ds_name=ds_name,
                    sample_idx=sample_idx,
                    model_name=args.model,
                    continuous_log_path=continuous_log_path,
                )
                
                # Initialize per-sample log
                log_path = Path(state["log_file_path"])
                log_path.write_text(f"Sample {sample_idx} - {ds_name}\n{'=' * 80}\n\n")
                
                # Invoke workflow
                print("Running workflow...")
                result = workflow.invoke(state)
                
                # Save sample results
                sample_output_dir = RESULTS_DIR / args.model / ds_name / f"sample_{sample_idx}"
                save_sample_results(result, sample_output_dir)
                
                # Append to CSV
                csv_path = RESULTS_DIR / args.model / f"{ds_name}_results.csv"
                append_to_csv(result, csv_path)
                
                # Log sample end
                cpp_logger.log_sample_end(result)
                
                # Print summary
                print(f"\n✓ Sample {sample_idx} complete")
                print(f"  Compilation:  {result['compilation_status']}")
                print(f"  Consistency:  {result['consistency_status']}")
                print(f"  Lock-Freedom: {result['lock_freedom_status']}")
                print(f"  Combined Score: {result['combined_score']:.4f}")
                
                print("\n--- GENERATED CODE ---")
                print(result.get("generated_code", "<no code generated>"))
                print("----------------------\n")
                
                # Store result for stats
                ds_results.append(result)
                
            except Exception as e:
                total_errors += 1
                error_msg = f"Error in sample {sample_idx}: {str(e)}"
                print(f"\n✗ {error_msg}")
                cpp_logger.log_error(error_msg)
                
                # Continue to next sample
                continue
        
        # Compute DS-level stats
        if ds_results:
            print(f"\n{'=' * 80}")
            print(f"STATISTICS: {ds_name.upper()}")
            print(f"{'=' * 80}\n")
            
            stats = compute_stats(ds_results)
            
            # Write stats file
            stats_path = RESULTS_DIR / args.model / f"{ds_name}_stats.txt"
            write_stats(stats, stats_path)
            
            # Write benchmark summary
            write_benchmark_summary(
                model=args.model,
                ds=ds_name,
                stats=stats,
                output_dir=RESULTS_DIR,
            )
            
            # Print summary to console
            print(f"Total Samples: {stats['total_samples']}")
            print(f"Compilation Pass Rate: {stats['pass_rates']['compilation']['rate']:.1%}")
            print(f"Consistency Pass Rate: {stats['pass_rates']['consistency']['rate']:.1%}")
            print(f"Lock-Freedom Rate: {stats['pass_rates']['lock_freedom']['rate']:.1%}")
            print(f"Average Combined Score: {stats['average_scores']['combined']:.4f}")
    
    # Log pipeline end
    print(f"\n{'=' * 80}")
    print("PIPELINE COMPLETE")
    print(f"{'=' * 80}\n")
    
    cpp_logger.log_section("PIPELINE COMPLETE")
    cpp_logger.log_info(f"Total samples: {total_samples}")
    cpp_logger.log_info(f"Total errors: {total_errors}")
    
    print(f"Total samples processed: {total_samples}")
    print(f"Total errors: {total_errors}")
    print(f"\nResults saved to: {RESULTS_DIR}")
    print(f"Continuous log: {continuous_log_path}")


# ── Main Entry Point ──────────────────────────────────────────────────────────

def main() -> None:
    """
    Main entry point for the C++ zero-shot pipeline runner.
    
    Requirements: AC 4.8.1-4.8.5 (Task 17)
    """
    # Parse arguments first (so --help works without env vars)
    args = parse_args()
    
    # Load environment variables
    load_dotenv()
    
    # Check for required environment variables
    if not os.getenv("NVIDIA_NIM_API_KEY"):
        print("ERROR: NVIDIA_NIM_API_KEY not found in environment")
        print("Please set NVIDIA_NIM_API_KEY in your .env file or environment")
        sys.exit(1)
        
    # Environment variable override if model is provided via CLI
    if args.model:
        os.environ["NVIDIA_NIM_MODEL"] = args.model
    else:
        args.model = os.environ.get("NVIDIA_NIM_MODEL", DEFAULT_MODEL)
    
    # Print banner
    print("\n" + "=" * 80)
    print("C++ ZERO-SHOT PIPELINE RUNNER")
    print("=" * 80)
    print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print(f"Model: {args.model}")
    print(f"Data Structures: {', '.join(args.ds)}")
    print(f"Samples per DS: {args.num_runs}")
    print("=" * 80 + "\n")
    
    try:
        # Run pipeline
        run_pipeline(args)
        
    except KeyboardInterrupt:
        print("\n\nPipeline interrupted by user")
        sys.exit(1)
        
    except Exception as e:
        print(f"\n\nFATAL ERROR: {str(e)}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
