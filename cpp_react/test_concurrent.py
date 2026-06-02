"""
cpp_react/test_concurrent.py — Concurrent C++ Testing Node
============================================================

Mirrors nodes/test_code.py (concurrent phase) for Java.
Reuses cpp_test_integration/ for compilation, consistency tests,
victim injection, and non-blocking tests.
"""
from __future__ import annotations

import os
import sys
from typing import Dict, Any
from datetime import datetime
from pathlib import Path

# Add project root to path for imports
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from .state import CppReactState


def node_test_conc(state: CppReactState) -> Dict[str, Any]:
    """
    Test concurrent C++ code: compile → consistency test → victim-inject → non-blocking test.
    Reuses cpp_test_integration/ infrastructure.
    """
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"
    code = state.get("generated_code", "")
    ds_name = state.get("data_structure", "linked_list")

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [CONCURRENT TEST] ---\n")
        f.write(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"DS: {ds_name}\n")
        f.flush()

    if not code or len(code.strip()) < 50:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write("  RESULT: FAIL — code is empty\n")
            f.flush()
        return {
            "test_result": "fail",
            "compilation_status": "fail",
            "consistency_status": "none",
            "lock_freedom_status": "none",
            "lock_syntax_status": "unknown",
            "error_message": "Code is empty or too short",
            "failure_stage": "compile",
        }

    # Import test infrastructure
    try:
        from cpp_test_integration.cpp_compiler import compile_cpp_code
        from cpp_test_integration.factory_modifier import modify_factory, restore_factory
        from cpp_test_integration.test_runner import run_consistency_test, run_nonblocking_test
        from cpp_test_integration.victim_injector import inject_victim_sleep
    except ImportError as e:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"  ERROR: Cannot import test infrastructure: {e}\n")
            f.flush()
        return {
            "test_result": "fail",
            "compilation_status": "none",
            "consistency_status": "none",
            "lock_freedom_status": "none",
            "lock_syntax_status": "unknown",
            "error_message": f"Import error: {e}",
            "failure_stage": "compile",
        }

    project_root = Path(__file__).resolve().parent.parent
    # The structures dir is where we write the generated .hpp
    structures_dir = project_root / "cpp_concurrent_testing" / "test"
    structures_dir.mkdir(parents=True, exist_ok=True)
    hpp_path = structures_dir / "ConcurrentDataStructure.hpp"
    # The factory is inside cpp_concurrent_testing/test/
    factory_path = project_root / "cpp_concurrent_testing" / "test" / "StructureFactory.hpp"

    # ── Stage 1: Write code to file ───────────────────────────────────────────
    try:
        hpp_path.write_text(code, encoding="utf-8")
    except Exception as e:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"  ERROR: Cannot write hpp: {e}\n")
            f.flush()
        return {
            "test_result": "fail",
            "compilation_status": "fail",
            "error_message": f"File write error: {e}",
            "failure_stage": "compile",
        }

    # ── Stage 2: Modify factory & compile ─────────────────────────────────────
    # modify_factory(hpp_path: Path, factory_path: Path)
    try:
        if factory_path.exists():
            modify_factory(hpp_path, factory_path)
        else:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write(f"  WARNING: Factory not found at {factory_path}, skipping factory modification\n")
                f.flush()
    except Exception as e:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write(f"  WARNING: Factory modification failed: {e}\n")
            f.flush()

    # compile_cpp_code(code: str, output_dir: Path) -> CompileResult
    compile_result = compile_cpp_code(code, structures_dir)
    compilation_ok = compile_result.status == "pass"
    compile_error = compile_result.error_message

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"  Compilation: {'PASS' if compilation_ok else 'FAIL'}\n")
        if compile_error:
            f.write(f"  Error: {compile_error[:500]}\n")
        f.flush()

    if not compilation_ok:
        try:
            if factory_path.exists():
                restore_factory(factory_path)
        except Exception:
            pass
        return {
            "test_result": "fail",
            "compilation_status": "fail",
            "consistency_status": "none",
            "lock_freedom_status": "none",
            "lock_syntax_status": "unknown",
            "error_message": f"Compilation failed: {compile_error[:500]}",
            "failure_stage": "compile",
        }

    # ── Stage 3: Consistency test ─────────────────────────────────────────────
    # run_consistency_test(algo, threads, duration, keyspace, test_dir, timeout)
    test_dir = project_root / "cpp_concurrent_testing"
    consistency_result = run_consistency_test(algo="generated", test_dir=test_dir)
    consistency_ok = consistency_result.status == "pass"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"  Consistency: {'PASS' if consistency_ok else 'FAIL'}\n")
        if not consistency_ok:
            f.write(f"  Detail: {consistency_result.details[:500]}\n")
        f.flush()

    if not consistency_ok:
        try:
            if factory_path.exists():
                restore_factory(factory_path)
        except Exception:
            pass
        return {
            "test_result": "fail",
            "compilation_status": "pass",
            "consistency_status": "fail",
            "lock_freedom_status": "none",
            "lock_syntax_status": "unknown",
            "error_message": f"Consistency test failed: {consistency_result.details[:500]}",
            "failure_stage": "first_sanity",
        }

    # ── Stage 4: Victim injection → non-blocking test ─────────────────────────
    # Lock syntax check (static analysis)
    lock_patterns = ['std::mutex', 'std::lock_guard', 'std::unique_lock']
    has_locks = any(p in code for p in lock_patterns)
    lock_syntax = "lock-based" if has_locks else "lock-free"

    # inject_victim_sleep(code: str) -> str  (returns modified code)
    victim_code = inject_victim_sleep(code)
    victim_ok = victim_code != code  # Injection happened if code changed

    if victim_ok:
        # Write victim-injected code and recompile
        hpp_path.write_text(victim_code, encoding="utf-8")
        recompile = compile_cpp_code(victim_code, structures_dir)
        if recompile.status == "pass":
            # run_nonblocking_test(algo, threads, duration, keyspace, test_dir, timeout)
            nb_result = run_nonblocking_test(algo="generated", test_dir=test_dir)
            nb_passed = nb_result.status == "pass"
            lock_freedom = "lock-free" if nb_passed else "lock-based"
        else:
            lock_freedom = "error"
    else:
        lock_freedom = "error"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"  Lock syntax : {lock_syntax}\n")
        f.write(f"  Lock freedom: {lock_freedom}\n")
        f.flush()

    # ── Restore factory ───────────────────────────────────────────────────────
    try:
        if factory_path.exists():
            restore_factory(factory_path)
    except Exception:
        pass

    # Restore original code (without victim)
    try:
        hpp_path.write_text(code, encoding="utf-8")
    except Exception:
        pass

    # ── Determine overall result ──────────────────────────────────────────────
    all_pass = consistency_ok and lock_freedom == "lock-free"

    if not consistency_ok:
        failure_stage = "first_sanity"
    elif lock_freedom != "lock-free":
        failure_stage = "second_sanity"
    else:
        failure_stage = "none"

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"  OVERALL: {'PASS' if all_pass else 'FAIL'} (stage={failure_stage})\n")
        f.flush()

    return {
        "test_result": "pass" if all_pass else "fail",
        "compilation_status": "pass",
        "consistency_status": "pass" if consistency_ok else "fail",
        "lock_freedom_status": lock_freedom,
        "lock_syntax_status": lock_syntax,
        "failure_stage": failure_stage,
        "error_message": "" if all_pass else f"Failed at {failure_stage}",
    }
