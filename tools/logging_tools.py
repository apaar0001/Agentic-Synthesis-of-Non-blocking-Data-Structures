"""
Logging tools for pipeline logging.
"""

from datetime import datetime
from typing import Dict, Any, Literal
from pathlib import Path


def log_to_file(message: str, log_file_path: str = "continuous_logs.txt") -> Dict[str, Any]:
    """
    Tool to log a message to both per-prompt log and continuous log.
    
    Args:
        message: The message to log
        log_file_path: Path to the per-prompt log file
        
    Returns:
        Dict with 'success'
    """
    try:
        continuous_log = "continuous_logs.txt"
        for log_path in [log_file_path, continuous_log]:
            with open(log_path, "a", encoding="utf-8") as f:
                f.write(message)
                f.flush()
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


def log_success(
    phase: Literal["seq", "conc"],
    attempt: int,
    compilation_status: str,
    sanity_status: str,
    lock_freedom_status: str = "none",
    prompt_topic: str = "unknown",
    log_file_path: str = "continuous_logs.txt"
) -> Dict[str, Any]:
    """
    Tool to log a success message.
    
    Args:
        phase: "seq" or "conc"
        attempt: Attempt number
        compilation_status: Compilation status
        sanity_status: Sanity test status
        lock_freedom_status: Lock-freedom test status (for concurrent phase)
        prompt_topic: Prompt topic name
        log_file_path: Path to per-prompt log file
        
    Returns:
        Dict with 'success'
    """
    timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    message = (
        f"=== [SUCCESS] ===\n"
        f"Timestamp: {timestamp_now}\n"
        f"Phase: {phase.upper()}\n"
        f"Attempt: {attempt}\n"
        f"Compilation Status: {compilation_status}\n"
        f"Sanity Status: {sanity_status}\n"
    )
    if phase == "conc":
        message += f"Lock-Freedom Status: {lock_freedom_status}\n"
    message += (
        f"Prompt Topic: {prompt_topic}\n"
        f"=== END SUCCESS ===\n\n"
    )
    
    return log_to_file(message, log_file_path)


def log_failure(
    phase: Literal["seq", "conc"],
    attempt: int,
    error_message: str,
    prompt_topic: str = "unknown",
    log_file_path: str = "continuous_logs.txt"
) -> Dict[str, Any]:
    """
    Tool to log a failure message.
    
    Args:
        phase: "seq" or "conc"
        attempt: Attempt number
        error_message: Error message
        prompt_topic: Prompt topic name
        log_file_path: Path to per-prompt log file
        
    Returns:
        Dict with 'success'
    """
    timestamp_now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    
    message = (
        f"=== [FAILURE] ===\n"
        f"Timestamp: {timestamp_now}\n"
        f"Phase: {phase.upper()}\n"
        f"Attempt: {attempt}\n"
        f"Error: {error_message}\n"
        f"Prompt Topic: {prompt_topic}\n"
        f"=== END FAILURE ===\n\n"
    )
    
    return log_to_file(message, log_file_path)
