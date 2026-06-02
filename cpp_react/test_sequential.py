"""
cpp_react/test_sequential.py — Sequential C++ Testing Node
============================================================

Mirrors nodes/test_code.py (sequential phase) for Java.
Compiles the sequential C++ code with clang++ to verify syntactic correctness.
"""
from __future__ import annotations

import os
import subprocess
import tempfile
from typing import Dict, Any
from datetime import datetime
from pathlib import Path

from .state import CppReactState


def node_test_code_seq(state: CppReactState) -> Dict[str, Any]:
    """Test sequential C++ code by compiling it."""
    log_file = state.get("log_file_path") or "continuous_logs_cpp.txt"
    code = state.get("generated_code", "")

    with open(log_file, "a", encoding="utf-8") as f:
        f.write(f"\n--- [SEQUENTIAL TEST] ---\n")
        f.write(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.flush()

    if not code or len(code.strip()) < 50:
        with open(log_file, "a", encoding="utf-8") as f:
            f.write("  RESULT: FAIL — code is empty or too short\n")
            f.flush()
        return {
            "test_result": "fail",
            "error_message": "Sequential code is empty or too short",
        }

    # Write code to temp file and compile
    with tempfile.TemporaryDirectory() as tmpdir:
        hpp_path = Path(tmpdir) / "ConcurrentDataStructure.hpp"

        # Create a minimal SetADT.hpp for the sequential test
        utils_dir = Path(tmpdir) / "utils"
        utils_dir.mkdir()
        (utils_dir / "SetADT.hpp").write_text(
            '#pragma once\n\n'
            'class SetADT {\n'
            'public:\n'
            '    virtual ~SetADT() = default;\n'
            '    virtual bool contains(int key) = 0;\n'
            '    virtual bool add(int key) = 0;\n'
            '    virtual bool remove(int key) = 0;\n'
            '};\n',
            encoding="utf-8",
        )

        # Fix include path for standalone compilation
        fixed_code = code.replace(
            '#include "../utils/SetADT.hpp"',
            '#include "utils/SetADT.hpp"'
        )
        hpp_path.write_text(fixed_code, encoding="utf-8")

        # Create a minimal test driver
        test_cpp = Path(tmpdir) / "test_driver.cpp"
        test_cpp.write_text(
            '#include "ConcurrentDataStructure.hpp"\n'
            'int main() {\n'
            '    ConcurrentDataStructure ds;\n'
            '    ds.add(1);\n'
            '    ds.contains(1);\n'
            '    ds.remove(1);\n'
            '    return 0;\n'
            '}\n',
            encoding="utf-8",
        )

        # Determine clang++ path (fallback for Windows if not on PATH)
        import shutil
        clang_cmd = "clang++"
        if not shutil.which("clang++"):
            fallback = r"C:\Program Files\LLVM\bin\clang++.exe"
            if os.path.exists(fallback):
                clang_cmd = fallback

        # Compile with clang++
        try:
            result = subprocess.run(
                [
                    clang_cmd, "-std=c++17", "-fsyntax-only",
                    "-I", str(tmpdir),
                    str(test_cpp),
                ],
                capture_output=True,
                text=True,
                timeout=30,
                cwd=tmpdir,
            )

            if result.returncode == 0:
                with open(log_file, "a", encoding="utf-8") as f:
                    f.write("  RESULT: PASS — sequential code compiles\n")
                    f.flush()
                return {
                    "test_result": "pass",
                    "error_message": "",
                }
            else:
                error_msg = result.stderr[:2000] if result.stderr else "Unknown compilation error"
                with open(log_file, "a", encoding="utf-8") as f:
                    f.write(f"  RESULT: FAIL — compilation error:\n{error_msg}\n")
                    f.flush()
                return {
                    "test_result": "fail",
                    "error_message": f"Compilation failed: {error_msg}",
                }

        except subprocess.TimeoutExpired:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write("  RESULT: FAIL — compilation timed out\n")
                f.flush()
            return {
                "test_result": "fail",
                "error_message": "Compilation timed out (30s)",
            }
        except FileNotFoundError:
            with open(log_file, "a", encoding="utf-8") as f:
                f.write("  RESULT: FAIL — clang++ not found\n")
                f.flush()
            return {
                "test_result": "fail",
                "error_message": "clang++ not found on PATH",
            }
