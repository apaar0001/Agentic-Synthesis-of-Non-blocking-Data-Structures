"""
Test Execution Node for C++ Zero-Shot Pipeline

This node orchestrates the testing phase of the pipeline:
1. Modifies StructureFactory.hpp to include the generated structure
2. Runs ConsistencyTest to verify correctness
3. Injects victim sleep to test lock-freedom
4. Runs NonBlockingTest to verify lock-freedom
5. Restores StructureFactory.hpp to original state
6. Updates state with all test results

Requirements: 4.4.1-4.4.5, 4.5.1-4.5.5
"""

from pathlib import Path
from typing import Dict
import logging

from cpp_zero_shot.cpp_state import CPPZeroShotState
from cpp_test_integration.factory_modifier import modify_factory, restore_factory
from cpp_test_integration.test_runner import run_consistency_test, run_nonblocking_test
from cpp_test_integration.victim_injector import inject_victim_sleep


def node_test_cpp(state: CPPZeroShotState) -> CPPZeroShotState:
    """
    Execute testing phase: ConsistencyTest and NonBlockingTest.
    
    This function:
    1. Writes generated code to ConcurrentDataStructure.hpp
    2. Modifies StructureFactory.hpp to include the generated structure
    3. Runs ConsistencyTest with 4 threads
    4. Updates state.consistency_status and state.stage_3_code
    5. Injects victim sleep for lock-freedom testing
    6. Runs NonBlockingTest with 8 threads
    7. Updates state.lock_freedom_status, state.lock_syntax_status
    8. Updates state.stage_4_code (victim-injected)
    9. Restores StructureFactory.hpp to original state
    10. Logs all results
    
    Args:
        state: Current pipeline state with generated_code populated
        
    Returns:
        Updated state with test results and stage snapshots
        
    Requirements:
        - AC 4.4.1: Pipeline dynamically modifies StructureFactory.hpp
        - AC 4.4.2: Pipeline recompiles test harnesses
        - AC 4.4.3: Pipeline runs ConsistencyTest with 4 threads
        - AC 4.4.4: Pipeline runs NonBlockingTest with 8 threads
        - AC 4.4.5: Test results parsed and captured
        - AC 4.5.1: Pipeline identifies CAS success points
        - AC 4.5.2: Pipeline injects sleep after CAS success
        - AC 4.5.3: Injected code compiles and runs
        - AC 4.5.4: Victim-injected code saved
        - AC 4.5.5: Semantic lock-freedom status captured
    """
    # Setup logging
    log_file = Path(state["log_file_path"])
    _log(log_file, "\n" + "="*80)
    _log(log_file, "STAGE 3: CONSISTENCY TEST")
    _log(log_file, "="*80)
    
    # Skip testing if compilation failed
    if state["compilation_status"] != "pass":
        _log(log_file, "Skipping tests: compilation failed")
        state["consistency_status"] = "none"
        state["lock_freedom_status"] = "none"
        state["lock_syntax_status"] = "unknown"
        state["stage_3_code"] = state["generated_code"]
        state["stage_4_code"] = state["generated_code"]
        return state
    
    # Determine paths
    test_dir = Path(__file__).parent.parent / "cpp_concurrent_testing"
    factory_path = test_dir / "test" / "StructureFactory.hpp"
    hpp_path = test_dir / "test" / "ConcurrentDataStructure.hpp"
    
    # Write generated code to ConcurrentDataStructure.hpp
    _log(log_file, f"\nWriting generated code to: {hpp_path}")
    try:
        hpp_path.write_text(state["generated_code"])
        _log(log_file, "✓ Code written successfully")
    except Exception as e:
        _log(log_file, f"✗ Failed to write code: {e}")
        state["consistency_status"] = "error"
        state["lock_freedom_status"] = "error"
        state["lock_syntax_status"] = "unknown"
        state["error_message"] += f"\nFailed to write code: {e}"
        state["stage_3_code"] = state["generated_code"]
        state["stage_4_code"] = state["generated_code"]
        return state
    
    # Modify StructureFactory.hpp
    _log(log_file, f"\nModifying factory: {factory_path}")
    try:
        modify_factory(hpp_path, factory_path)
        _log(log_file, "✓ Factory modified successfully")
    except Exception as e:
        _log(log_file, f"✗ Failed to modify factory: {e}")
        state["consistency_status"] = "error"
        state["lock_freedom_status"] = "error"
        state["lock_syntax_status"] = "unknown"
        state["error_message"] += f"\nFailed to modify factory: {e}"
        state["stage_3_code"] = state["generated_code"]
        state["stage_4_code"] = state["generated_code"]
        return state
    
    try:
        # Run ConsistencyTest
        _log(log_file, "\nRunning ConsistencyTest...")
        _log(log_file, "Command: ./bin/ConsistencyTest -a generated -n 4 -d 5 -k 100")
        
        consistency_result = run_consistency_test(
            algo="generated",
            threads=4,
            duration=5,
            keyspace=100,
            test_dir=test_dir,
            timeout=60
        )
        
        _log(log_file, f"Status: {consistency_result.status}")
        _log(log_file, f"Details: {consistency_result.details}")
        _log(log_file, f"Exit code: {consistency_result.exit_code}")
        
        if consistency_result.output:
            _log(log_file, f"\nStdout:\n{consistency_result.output}")
        if consistency_result.error:
            _log(log_file, f"\nStderr:\n{consistency_result.error}")
        
        # Update state with consistency results
        state["consistency_status"] = consistency_result.status
        if consistency_result.status in ["fail", "error", "timeout"]:
            state["error_message"] += f"\nConsistency test {consistency_result.status}: {consistency_result.details}"
        
        # Save stage 3 code (post-consistency)
        state["stage_3_code"] = state["generated_code"]
        
        # Stage 4: Non-Blocking Test with Victim Injection
        _log(log_file, "\n" + "="*80)
        _log(log_file, "STAGE 4: NON-BLOCKING TEST (VICTIM INJECTION)")
        _log(log_file, "="*80)
        
        # Inject victim sleep
        _log(log_file, "\nInjecting victim sleep...")
        try:
            victim_code = inject_victim_sleep(state["generated_code"])
            _log(log_file, "✓ Victim sleep injected successfully")
            
            # Write victim-injected code
            hpp_path.write_text(victim_code)
            _log(log_file, f"✓ Victim code written to: {hpp_path}")
            
            # Save stage 4 code (victim-injected)
            state["stage_4_code"] = victim_code
            
        except Exception as e:
            _log(log_file, f"✗ Failed to inject victim sleep: {e}")
            state["lock_freedom_status"] = "error"
            state["lock_syntax_status"] = "unknown"
            state["error_message"] += f"\nFailed to inject victim sleep: {e}"
            state["stage_4_code"] = state["generated_code"]
            return state
        
        # Run NonBlockingTest
        _log(log_file, "\nRunning NonBlockingTest...")
        _log(log_file, "Command: ./bin/NonBlockingTest -a generated -n 8 -d 5 -k 100")
        
        nonblocking_result = run_nonblocking_test(
            algo="generated",
            threads=8,
            duration=5,
            keyspace=100,
            test_dir=test_dir,
            timeout=60
        )
        
        _log(log_file, f"Status: {nonblocking_result.status}")
        _log(log_file, f"Details: {nonblocking_result.details}")
        _log(log_file, f"Exit code: {nonblocking_result.exit_code}")
        
        if nonblocking_result.output:
            _log(log_file, f"\nStdout:\n{nonblocking_result.output}")
        if nonblocking_result.error:
            _log(log_file, f"\nStderr:\n{nonblocking_result.error}")
        
        # Determine lock-freedom status
        # If NonBlockingTest passes with victim injection, it's lock-free
        # If it fails, it's lock-based (victim thread blocked progress)
        if nonblocking_result.status == "pass":
            state["lock_freedom_status"] = "lock-free"
            _log(log_file, "\n✓ Structure is LOCK-FREE (passed with victim injection)")
        elif nonblocking_result.status == "fail":
            state["lock_freedom_status"] = "lock-based"
            _log(log_file, "\n✗ Structure is LOCK-BASED (failed with victim injection)")
        else:
            state["lock_freedom_status"] = "error"
            _log(log_file, f"\n✗ Non-blocking test error: {nonblocking_result.status}")
        
        # Determine syntactic lock status (check for mutex/lock keywords)
        state["lock_syntax_status"] = _check_lock_syntax(state["generated_code"])
        _log(log_file, f"Syntactic lock detection: {state['lock_syntax_status']}")
        
        if nonblocking_result.status in ["fail", "error", "timeout"]:
            state["error_message"] += f"\nNon-blocking test {nonblocking_result.status}: {nonblocking_result.details}"
    
    finally:
        # Always restore factory
        _log(log_file, "\nRestoring factory...")
        try:
            restore_factory(factory_path)
            _log(log_file, "✓ Factory restored successfully")
        except Exception as e:
            _log(log_file, f"✗ Failed to restore factory: {e}")
            state["error_message"] += f"\nFailed to restore factory: {e}"
    
    _log(log_file, "\n" + "="*80)
    _log(log_file, "TEST EXECUTION COMPLETE")
    _log(log_file, f"Consistency: {state['consistency_status']}")
    _log(log_file, f"Lock-freedom (semantic): {state['lock_freedom_status']}")
    _log(log_file, f"Lock-freedom (syntactic): {state['lock_syntax_status']}")
    _log(log_file, "="*80)
    
    return state


def _check_lock_syntax(code: str) -> str:
    """
    Check for syntactic indicators of locks in the code.
    
    Args:
        code: C++ source code
        
    Returns:
        "lock-based" if lock keywords found, "lock-free" otherwise
    """
    lock_keywords = [
        "std::mutex",
        "std::lock_guard",
        "std::unique_lock",
        "std::shared_lock",
        "pthread_mutex",
        ".lock()",
        ".unlock()"
    ]
    
    code_lower = code.lower()
    for keyword in lock_keywords:
        if keyword.lower() in code_lower:
            return "lock-based"
    
    return "lock-free"


def _log(log_file: Path, message: str) -> None:
    """
    Append a message to the log file.
    
    Args:
        log_file: Path to log file
        message: Message to log
    """
    try:
        with open(log_file, 'a') as f:
            f.write(message + '\n')
    except Exception as e:
        # Fallback to console if file logging fails
        print(f"[LOG ERROR] {e}")
        print(message)
