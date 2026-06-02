"""
Continuous logging for C++ Zero-Shot Pipeline.

This module provides functions for managing the continuous log file that tracks
progress across all samples. It includes functions for initializing the log,
logging section headers, sample start/end, stage results, and merging per-sample
logs into the continuous log.

Requirements: AC 4.8.3, 4.9.5 (Task 16)
"""
from __future__ import annotations

from pathlib import Path
from datetime import datetime
from typing import Optional, Dict, Any

from .cpp_state import CPPZeroShotState


# Global continuous log path (set by init_continuous_log)
_CONTINUOUS_LOG_PATH: Optional[Path] = None


# ── Initialization ────────────────────────────────────────────────────────────

def init_continuous_log(log_path: Path) -> None:
    """
    Initialize the continuous log file.
    
    Creates a new log file with a header and timestamp. If the file already
    exists, it will be overwritten.
    
    Args:
        log_path: Path to continuous log file
        
    Requirements: AC 4.8.3, 4.9.5 (Task 16.1)
    """
    global _CONTINUOUS_LOG_PATH
    _CONTINUOUS_LOG_PATH = log_path
    
    # Create parent directory if needed
    log_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Write header
    lines = [
        "=" * 80,
        "C++ ZERO-SHOT PIPELINE - CONTINUOUS LOG",
        "=" * 80,
        f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    
    log_path.write_text("\n".join(lines) + "\n")


# ── Section Logging ───────────────────────────────────────────────────────────

def log_section(title: str) -> None:
    """
    Log a section header to the continuous log.
    
    Adds a formatted section header with timestamp.
    
    Args:
        title: Section title (e.g., "LINKED LIST SAMPLES")
        
    Requirements: AC 4.9.5 (Task 16.1)
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    lines = [
        "",
        "=" * 80,
        f"{title}",
        "=" * 80,
        f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    
    _append_to_log("\n".join(lines))


# ── Sample Logging ────────────────────────────────────────────────────────────

def log_sample_start(ds: str, sample_idx: int) -> None:
    """
    Log the start of a sample to the continuous log.
    
    Args:
        ds: Data structure name
        sample_idx: Sample index (1-indexed)
        
    Requirements: AC 4.9.5 (Task 16.1)
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    lines = [
        "",
        "-" * 80,
        f"Sample {sample_idx} - {ds}",
        "-" * 80,
        f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
    ]
    
    _append_to_log("\n".join(lines))


def log_sample_end(state: CPPZeroShotState) -> None:
    """
    Log the end of a sample to the continuous log.
    
    Includes a summary of all stage results and final scores.
    
    Args:
        state: Pipeline state containing all results
        
    Requirements: AC 4.9.5 (Task 16.1)
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    lines = [
        "",
        f"Completed: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}",
        "",
        "Results:",
        f"  Compilation:  {state['compilation_status']}",
        f"  Consistency:  {state['consistency_status']}",
        f"  Lock-Freedom: {state['lock_freedom_status']}",
        f"  Combined Score: {state['combined_score']:.4f}",
        "",
    ]
    
    _append_to_log("\n".join(lines))


# ── Stage Logging ─────────────────────────────────────────────────────────────

def log_stage_result(stage: str, status: str, details: Optional[Dict[str, Any]] = None) -> None:
    """
    Log the result of a pipeline stage to the continuous log.
    
    Args:
        stage: Stage name (e.g., "Generation", "Compilation", "Testing")
        status: Stage status (e.g., "pass", "fail", "complete")
        details: Optional dictionary with additional details
        
    Requirements: AC 4.9.5 (Task 16.1)
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    timestamp = datetime.now().strftime('%H:%M:%S')
    lines = [f"[{timestamp}] {stage}: {status}"]
    
    if details:
        for key, value in details.items():
            lines.append(f"  {key}: {value}")
    
    _append_to_log("\n".join(lines))


# ── Log Merging ───────────────────────────────────────────────────────────────

def merge_sample_log(sample_log_path: Path) -> None:
    """
    Merge a per-sample log file into the continuous log.
    
    Reads the sample log file and appends its contents to the continuous log
    with a header indicating it's a merged log.
    
    Args:
        sample_log_path: Path to per-sample log file
        
    Requirements: AC 4.9.5 (Task 16.1)
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    if not sample_log_path.exists():
        _append_to_log(f"\n[WARNING] Sample log not found: {sample_log_path}\n")
        return
    
    # Read sample log
    sample_log_content = sample_log_path.read_text()
    
    # Append to continuous log with header
    lines = [
        "",
        "--- Per-Sample Log (Merged) ---",
        sample_log_content,
        "--- End Per-Sample Log ---",
        "",
    ]
    
    _append_to_log("\n".join(lines))


# ── Helper Functions ──────────────────────────────────────────────────────────

def _append_to_log(text: str) -> None:
    """
    Append text to the continuous log file.
    
    Args:
        text: Text to append
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    with open(_CONTINUOUS_LOG_PATH, "a", encoding="utf-8") as f:
        f.write(text + "\n")


def log_error(error_msg: str) -> None:
    """
    Log an error message to the continuous log.
    
    Args:
        error_msg: Error message to log
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    timestamp = datetime.now().strftime('%H:%M:%S')
    lines = [
        "",
        f"[{timestamp}] ERROR: {error_msg}",
        "",
    ]
    
    _append_to_log("\n".join(lines))


def log_info(info_msg: str) -> None:
    """
    Log an info message to the continuous log.
    
    Args:
        info_msg: Info message to log
    """
    if _CONTINUOUS_LOG_PATH is None:
        return
    
    timestamp = datetime.now().strftime('%H:%M:%S')
    _append_to_log(f"[{timestamp}] {info_msg}")
