"""
Code compilation tools for Java code.
"""

import subprocess
import os
from pathlib import Path
from typing import Dict, Any


PROJECT_ROOT = Path(__file__).resolve().parents[1] / "java_concurrent_testing"
CONCURRENT_DS_PATH = PROJECT_ROOT / "src" / "main" / "java" / "com" / "example" / "Sets" / "ConcurrentDataStructure.java"
SEQUENTIAL_DS_PATH = PROJECT_ROOT / "src" / "main" / "java" / "com" / "example" / "Sets" / "SequentialDataStructure.java"


def write_code_to_file(code: str, phase: str = "conc") -> Dict[str, Any]:
    """
    Tool to write generated code to the appropriate file for compilation.
    
    Args:
        code: The Java code to write
        phase: "seq" for sequential or "conc" for concurrent
        
    Returns:
        Dict with 'success' and optional 'error'
    """
    try:
        if phase == "seq":
            SEQUENTIAL_DS_PATH.parent.mkdir(parents=True, exist_ok=True)
            with open(SEQUENTIAL_DS_PATH, "w", encoding="utf-8") as f:
                f.write(code)
        else:
            CONCURRENT_DS_PATH.parent.mkdir(parents=True, exist_ok=True)
            with open(CONCURRENT_DS_PATH, "w", encoding="utf-8") as f:
                f.write(code)
        return {"success": True}
    except Exception as e:
        return {"success": False, "error": str(e)}


def compile_java_code() -> Dict[str, Any]:
    """
    Tool to compile Java code using Maven.
    
    Returns:
        Dict with 'success', 'returncode', 'stdout', 'stderr'
    """
    user_home = os.path.expanduser("~")
    mvn_paths = [
        "mvn",
        f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd",
        "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"
    ]
    
    for mvn_path in mvn_paths:
        try:
            result = subprocess.run(
                [mvn_path, "-q", "compile", "-f", str(PROJECT_ROOT / "pom.xml")],
                capture_output=True,
                text=True,
                cwd=str(PROJECT_ROOT),
                timeout=30
            )
            return {
                "success": result.returncode == 0,
                "returncode": result.returncode,
                "stdout": result.stdout,
                "stderr": result.stderr
            }
        except FileNotFoundError:
            continue
        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "returncode": 1,
                "stdout": "",
                "stderr": "Compilation timed out after 30 seconds"
            }
    
    return {
        "success": False,
        "returncode": 1,
        "stdout": "",
        "stderr": "Maven not found in PATH or standard locations"
    }
