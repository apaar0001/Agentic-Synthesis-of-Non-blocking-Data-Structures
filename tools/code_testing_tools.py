"""
Code testing tools for running benchmarks and lock-freedom tests.
"""

import subprocess
import os
from pathlib import Path
from typing import Dict, Any, Literal


PROJECT_ROOT = Path(__file__).resolve().parents[1] / "java_concurrent_testing"


def run_sequential_tests() -> Dict[str, Any]:
    """
    Tool to run sequential benchmark tests.
    
    Returns:
        Dict with 'success', 'passed', 'output', 'error'
    """
    user_home = os.path.expanduser("~")
    mvn_paths = [
        "mvn",
        f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd",
        "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"
    ]
    main_class = "com.example.test.BenchmarkSequential"
    
    for mvn_path in mvn_paths:
        try:
            cmd = [
                mvn_path,
                "-q",
                "exec:java",
                f"-Dexec.mainClass={main_class}",
                "-f", str(PROJECT_ROOT / "pom.xml")
            ]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=str(PROJECT_ROOT),
                timeout=30
            )
            output = (result.stdout or "") + (result.stderr or "")
            passed = "Sanity Test Passed" in output
            
            return {
                "success": True,
                "passed": passed,
                "output": output,
                "returncode": result.returncode,
                "error": "" if passed else output
            }
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "passed": False,
                "output": "",
                "returncode": 1,
                "error": "Test timed out after 30 seconds"
            }
    
    return {
        "success": False,
        "passed": False,
        "output": "",
        "returncode": 1,
        "error": "Maven not found in PATH or standard locations"
    }


def run_concurrent_tests() -> Dict[str, Any]:
    """
    Tool to run concurrent benchmark tests.
    
    Returns:
        Dict with 'success', 'passed', 'output', 'error'
    """
    user_home = os.path.expanduser("~")
    mvn_paths = [
        "mvn",
        f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd",
        "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"
    ]
    main_class = "com.example.test.ConsistencyTest"
    
    for mvn_path in mvn_paths:
        try:
            cmd = [
                mvn_path,
                "-q",
                "exec:java",
                f"-Dexec.mainClass={main_class}",
                "-f", str(PROJECT_ROOT / "pom.xml")
            ]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                cwd=str(PROJECT_ROOT),
                timeout=30
            )
            output = (result.stdout or "") + (result.stderr or "")
            passed = "Sanity Test Passed" in output
            
            return {
                "success": True,
                "passed": passed,
                "output": output,
                "returncode": result.returncode,
                "error": "" if passed else output
            }
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "passed": False,
                "output": "",
                "returncode": 1,
                "error": "Test timed out after 30 seconds"
            }

    return {
        "success": False,
        "passed": False,
        "output": "",
        "returncode": 1,
        "error": "Maven not found in PATH or standard locations"
    }


def run_lock_freedom_test() -> Dict[str, Any]:
    """
    Tool to run lock-freedom test for concurrent data structures.
    
    Returns:
        Dict with 'status' ('lock-free', 'lock-based', 'error'), 'output', 'error'
    """
    user_home = os.path.expanduser("~")
    mvn_paths = [
        "mvn",
        f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd",
        "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"
    ]
    main_class = "com.example.test.LockFreedomTest"
    java_paths = [
        "java",
        f"C:\\Program Files\\Microsoft\\jdk-17.0.17.10-hotspot\\bin\\java.exe",
        f"C:\\Program Files\\Java\\jdk-17\\bin\\java.exe"
    ]
    
    # First, ensure the code is compiled
    for mvn_path in mvn_paths:
        try:
            compile_cmd = [mvn_path, "-q", "compile", "-f", str(PROJECT_ROOT / "pom.xml")]
            subprocess.run(compile_cmd, capture_output=True, text=True, cwd=str(PROJECT_ROOT), timeout=30)
            break
        except (FileNotFoundError, subprocess.TimeoutExpired):
            continue
    
    # Build classpath
    classpath_parts = [
        str(PROJECT_ROOT / "target" / "classes"),
    ]
    
    lib_dir = PROJECT_ROOT / "lib"
    if lib_dir.exists():
        for jar_file in lib_dir.glob("*.jar"):
            classpath_parts.append(str(jar_file))
    
    classpath = ";".join(classpath_parts)
    
    # Try running with java directly
    for java_path in java_paths:
        try:
            java_cmd = [
                java_path,
                "-cp", classpath,
                main_class
            ]
            result = subprocess.run(
                java_cmd,
                capture_output=True,
                text=True,
                cwd=str(PROJECT_ROOT),
                timeout=8
            )
            output = (result.stdout or "") + (result.stderr or "")
            
            if "RESULT: LOCK-FREE" in output or "LOCK-FREE (or better)" in output:
                return {
                    "status": "lock-free",
                    "output": output,
                    "error": ""
                }
            elif "RESULT: LOCK-BASED" in output or "LOCK-BASED" in output:
                return {
                    "status": "lock-based",
                    "output": output,
                    "error": ""
                }
            else:
                return {
                    "status": "error",
                    "output": output,
                    "error": f"Unexpected output: {output}"
                }
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            return {
                "status": "error",
                "output": "",
                "error": "Lock-freedom test timed out after 8 seconds"
            }
        except Exception as e:
            continue
    
    return {
        "status": "error",
        "output": "",
        "error": "Lock-freedom test: Could not execute test"
    }
