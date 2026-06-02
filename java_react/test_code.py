from typing import Dict, Any
import subprocess
import os
import time
from pathlib import Path
from .state import GraphState
from .conversation_history import ConversationHistory, extract_text_from_response
import hashlib
import re
from langsmith import traceable
from langchain_openai import ChatOpenAI
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser


PROJECT_ROOT = Path(os.path.dirname(os.path.abspath(__file__))).parent / "java_concurrent_testing"
CONCURRENT_DS_PATH = PROJECT_ROOT / "src" / "main" / "java" / "com" / "example" / "Sets" / "ConcurrentDataStructure.java"
SEQUENTIAL_DS_PATH = PROJECT_ROOT / "src" / "main" / "java" / "com" / "example" / "Sets" / "SequentialDataStructure.java"
_TEST_DIR = PROJECT_ROOT / "src" / "main" / "java" / "com" / "example" / "test"
# Concurrent-dependent test sources that should be hidden during sequential phase
CONC_DEPENDENT_SOURCES = [
    CONCURRENT_DS_PATH,
    _TEST_DIR / "ConsistencyTest.java",
    _TEST_DIR / "NonBlockingTest.java",
    _TEST_DIR / "LockFreedomTest.java",
    _TEST_DIR / "QueueConsistencyTest.java",
    _TEST_DIR / "QueueNonBlockingTest.java",
    _TEST_DIR / "StackConsistencyTest.java",
    _TEST_DIR / "StackNonBlockingTest.java",
]
# DS-specific sequential benchmark files (each assumes a specific ADT)
_BENCH_SEQ_SET_PATH = _TEST_DIR / "BenchmarkSequential.java"
_BENCH_SEQ_QUEUE_PATH = _TEST_DIR / "BenchmarkSequentialQueue.java"
_BENCH_SEQ_STACK_PATH = _TEST_DIR / "BenchmarkSequentialStack.java"

# DS type set by node_test_code_conc/node_test_code_seq so _run_bench can pick the right test class.
_CURRENT_DS_TYPE: str = "set"  # "set" | "queue" | "stack"

# DS names that use Queue semantics
_QUEUE_DS = {"queue"}
# DS names that use Stack semantics
_STACK_DS = {"stack"}


def _wrong_ds_benchmarks(ds_type: str) -> list:
    """Return benchmark files that conflict with the given DS type and must be hidden during compilation."""
    if ds_type == "queue":
        return [_BENCH_SEQ_SET_PATH, _BENCH_SEQ_STACK_PATH]
    elif ds_type == "stack":
        return [_BENCH_SEQ_SET_PATH, _BENCH_SEQ_QUEUE_PATH]
    else:  # set
        return [_BENCH_SEQ_QUEUE_PATH, _BENCH_SEQ_STACK_PATH]


def _inject_victim_sleep(java_code: str, ds_type: str = "set") -> str:
	"""
	Post-process the generated concurrent Java code to inject:
	1. A global _lfVictimChosen / _lfRetired flag pair (class-level fields)
	2. Helper _lfShouldStall() method
	3. Early-exit guards in the right public methods
	4. Victim stall injection after the linearisation-point comment:
	   - SET  DS: // Node has been marked  (inside remove())
	   - QUEUE DS: // Dequeue victim point  (inside dequeue())
	   - STACK DS: // Pop victim point      (inside pop())

	ds_type: "set" | "queue" | "stack"
	"""
	if not java_code:
		return java_code

	# Idempotency check
	if "_lfVictimChosen" in java_code and "_lfRetired" in java_code:
		return java_code

	# Determine victim anchor and guarded methods per DS type
	if ds_type == "queue":
		victim_anchor = "// Dequeue victim point"
		guarded_methods = (
			"public void enqueue(",
			"public int dequeue(",
			"public boolean isEmpty(",
		)
		victim_log_label = "dequeue()"
		victim_return = "return -1;"
	elif ds_type == "stack":
		victim_anchor = "// Pop victim point"
		guarded_methods = (
			"public void push(",
			"public int pop(",
			"public boolean isEmpty(",
		)
		victim_log_label = "pop()"
		victim_return = "return -1;"
	else:  # set / default
		victim_anchor = "// Node has been marked"
		guarded_methods = (
			"public boolean add(int key)",
			"public boolean remove(int key)",
			"public boolean contains(int key)",
		)
		victim_log_label = "remove()"
		victim_return = "return false;"

	lines = java_code.splitlines()
	new_lines: list[str] = []
	inserted_helpers = False
	inserted_in_remove = False

	for i, line in enumerate(lines):
		# -------------------------------------------------
		# Inject helper fields/methods after class header
		# -------------------------------------------------
		if (not inserted_helpers) and "class ConcurrentDataStructure" in line:
			new_lines.append(line)
			new_lines.append("    // Lock-freedom test helpers (auto-injected)")
			new_lines.append("    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);")
			new_lines.append("    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);")
			new_lines.append("    private static final ThreadLocal<Boolean> _lfRetired =")
			new_lines.append("            ThreadLocal.withInitial(() -> false);")
			new_lines.append("")
			new_lines.append("    /**")
			new_lines.append("     * Decide if the *current* thread should become the victim.")
			new_lines.append("     *")
			new_lines.append("     * Each call increments a per-thread operation counter. Once a thread")
			new_lines.append("     * has executed more than 100 operations and no victim has been chosen,")
			new_lines.append("     * it atomically claims the victim role and will then stall.")
			new_lines.append("     */")
			new_lines.append("    private static boolean _lfShouldStall() {")
			new_lines.append("        int c = _lfOpCount.get() + 1;")
			new_lines.append("        _lfOpCount.set(c);")
			new_lines.append("        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {")
			new_lines.append("            return true;")
			new_lines.append("        }")
			new_lines.append("        return false;")
			new_lines.append("    }")
			new_lines.append("")
			inserted_helpers = True
			continue

		new_lines.append(line)

		# -------------------------------------------------
		# Inject early-exit guard in public operations
		# -------------------------------------------------
		if any(m in line for m in guarded_methods):
			# For void methods (enqueue/push) we cannot return false; skip guard
			# For int-return or boolean-return methods we inject a short-circuit.
			if "public void" not in line:
				new_lines.append("        if (_lfRetired.get()) {")
				new_lines.append(f"            {victim_return}")
				new_lines.append("        }")

		# -------------------------------------------------
		# Inject stall + retirement logic after victim anchor
		# -------------------------------------------------
		if (not inserted_in_remove) and victim_anchor in line:
			new_lines.append("            // Lock-freedom victim stall injection (auto-injected)")
			new_lines.append("            if (_lfShouldStall()) {")
			new_lines.append(f'                System.err.println("LOG: Victim thread stalling inside {victim_log_label}");')
			new_lines.append("                try {")
			new_lines.append("                    Thread.sleep(10_000);")
			new_lines.append("                } catch (InterruptedException ignored) {")
			new_lines.append("                }")
			new_lines.append("                System.err.println(\"LOG: Victim resumed and retiring\");")
			new_lines.append("                _lfRetired.set(true);")
			new_lines.append(f"                {victim_return}")
			new_lines.append("            }")
			inserted_in_remove = True

	return "\n".join(new_lines)



def _extract_java_code(generated_code: str) -> str:
	"""Extract Java code from generated text, removing markdown formatting."""
	lines = generated_code.split('\n')
	java_lines = []
	in_java_block = False

	for line in lines:
		# Start of Java code block
		if line.strip().startswith('```java'):
			in_java_block = True
			continue
		# End of code block
		elif line.strip() == '```' and in_java_block:
			break
		# Regular Java code
		elif in_java_block:
			java_lines.append(line)
		# Look for Java code without markdown (fallback)
		elif not in_java_block and (line.strip().startswith('package ') or
									line.strip().startswith('import ') or
									line.strip().startswith('public class ') or
									line.strip().startswith('class ') or
									line.strip().startswith('interface ')):
			java_lines.append(line)
			in_java_block = True
	
	return '\n'.join(java_lines)


def _write_code(generated_code: str, phase: str = "conc") -> None:
	"""Write code to appropriate file based on phase."""
	code = _sanitize_code(generated_code, phase)
	if phase == "seq":
		SEQUENTIAL_DS_PATH.parent.mkdir(parents=True, exist_ok=True)
		# Write exactly what the generator produced (already validated),
		# to avoid altering content between manual and pipeline runs
		with open(SEQUENTIAL_DS_PATH, "w", encoding="utf-8") as f:
			f.write(code)
	else:
		# For concurrent phase, write to ConcurrentDataStructure.java
		CONCURRENT_DS_PATH.parent.mkdir(parents=True, exist_ok=True)
		# Write exactly what the generator produced (already validated),
		# to avoid altering content between manual and pipeline runs
		with open(CONCURRENT_DS_PATH, "w", encoding="utf-8") as f:
			f.write(code)


def _sanitize_code(code: str, phase: str) -> str:
	"""
	Reduce compile collisions:
	- Enforce package com.example.Sets
	- Enforce class name SequentialDataStructure / ConcurrentDataStructure
	"""
	if not code:
		return code
	target_package = "package com.example.Sets;"
	cls_name = "SequentialDataStructure" if phase == "seq" else "ConcurrentDataStructure"
	node_name = "SeqNode" if phase == "seq" else "ConcNode"

	# Ensure package
	if "package " in code:
		code = re.sub(r"package\s+[^\n;]+;", target_package, code, count=1)
	else:
		code = target_package + "\n\n" + code

	# Fix class name
	code = re.sub(r"class\s+SequentialDataStructure", f"class {cls_name}", code, count=1)
	code = re.sub(r"class\s+ConcurrentDataStructure", f"class {cls_name}", code, count=1)

	return code


def _log_file_snapshot(log_file: str, file_path: Path, label: str) -> None:
	try:
		content = file_path.read_text(encoding="utf-8")
		digest = hashlib.sha256(content.encode("utf-8")).hexdigest()
		code_lines = "\n".join(content.splitlines())
		with open(log_file, "a", encoding="utf-8") as f:
			f.write(f"FILE SNAPSHOT ({label}): {file_path}\n")
			f.write(f"SHA256: {digest}\n")
			f.write("--- BEGIN PREVIEW ---\n")
			f.write(code_lines + "\n")
			f.write("--- END PREVIEW ---\n\n")
			f.flush()
	except Exception as _:
		pass


def _compile(timeout_seconds: int = 120) -> subprocess.CompletedProcess:
    # Try different Maven paths
    user_home = os.path.expanduser("~")
    mvn_paths = ["mvn", f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd", "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"]
    for mvn_path in mvn_paths:
        try:
            try:
                result = subprocess.run(
                    [mvn_path, "-q", "compile", "-f", str(PROJECT_ROOT / "pom.xml")],
                    capture_output=True,
                    text=True,
                    cwd=str(PROJECT_ROOT),
                    timeout=timeout_seconds,
                )
                return result
            except subprocess.TimeoutExpired as e:
                out = (e.stdout or "") + (e.stderr or "")
                return subprocess.CompletedProcess([], 1, e.stdout or "", f"Compile timed out after {timeout_seconds}s\n{out}")
        except FileNotFoundError:
            continue
    return subprocess.CompletedProcess([], 1, "", "Maven not found in PATH or standard locations")


def _compile_reprompt(
    code: str,
    compile_error: str,
    phase: str,
    log_file: str,
    conversation_history: list,
) -> tuple:
    """
    Stateful compile reprompt: adds the compile error as the next user turn in
    the existing conversation, then calls the LLM with full history.

    Returns (fixed_java_code: str, updated_history: list).
    The code is already in the prior assistant turn — no need to paste it again.
    """
    _write_to_logs(log_file, "[COMPILE REPROMPT] Compilation failed. Attempting one-time LLM fix...\n")

    fix_prompt = (
        "The Java code above failed to compile.\n"
        "Compilation error:\n" + (compile_error or "<no error output>") + "\n\n"
        "Fix the errors and return the complete corrected Java file. Output only code."
    )

    try:
        api_key = os.environ.get("NVIDIA_NIM_API_KEY")
        model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
        llm = ChatOpenAI(
            base_url="https://integrate.api.nvidia.com/v1",
            api_key=api_key,
            model=model,
            temperature=0.2,
            max_tokens=3000,
        )

        # Continue the existing conversation — no new [system, user] wrapper
        history = ConversationHistory.from_dict_list(conversation_history)
        history.add_user(fix_prompt)
        _write_to_logs(log_file, f"[COMPILE REPROMPT] Sending {len(history)} history turns to LLM.\n")

        response = llm.invoke(history.to_langchain_messages())
        raw_response = extract_text_from_response(response)
        history.add_assistant(raw_response)

        fixed_code = _extract_java_code(raw_response)
        fixed_code = _sanitize_code(fixed_code, phase)

        _write_to_logs(log_file, f"[COMPILE REPROMPT] LLM returned fixed code ({len(fixed_code)} chars).\n")
        return fixed_code, history.to_dict_list()
    except Exception as e:
        _write_to_logs(log_file, f"[COMPILE REPROMPT] LLM call failed: {e}\n")
        return "", conversation_history



def _run_lock_freedom_test() -> subprocess.CompletedProcess:
	"""Run the lock-freedom test for concurrent data structures"""
	user_home = os.path.expanduser("~")
	mvn_paths = ["mvn", f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd", "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"]
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
	
	# Build classpath - include target/classes and all JARs from lib
	classpath_parts = [
		str(PROJECT_ROOT / "target" / "classes"),
	]
	
	# Add all JAR files from lib directory
	lib_dir = PROJECT_ROOT / "lib"
	if lib_dir.exists():
		for jar_file in lib_dir.glob("*.jar"):
			classpath_parts.append(str(jar_file))
	
	classpath = ";".join(classpath_parts)
	
	# Try running with java directly (more reliable than Maven exec)
	for java_path in java_paths:
		try:
			java_cmd = [
				java_path,
				"-cp", classpath,
				main_class
			]
			result = subprocess.run(java_cmd, capture_output=True, text=True, cwd=str(PROJECT_ROOT), timeout=8)
			# Check if we got valid output (LockFreedomTest prints "RESULT: ...")
			output = (result.stdout or "") + (result.stderr or "")
			if "RESULT:" in output:
				return result
		except FileNotFoundError:
			continue
		except subprocess.TimeoutExpired:
			return subprocess.CompletedProcess([], 1, "", "Lock-freedom test timed out after 8 seconds")
		except Exception as e:
			continue
	
	# Fallback: try Maven exec plugin
	for mvn_path in mvn_paths:
		try:
			cmd = [
				mvn_path,
				"-q",
				"exec:java",
				f"-Dexec.mainClass={main_class}",
				"-f", str(PROJECT_ROOT / "pom.xml")
			]
			result = subprocess.run(cmd, capture_output=True, text=True, cwd=str(PROJECT_ROOT), timeout=8)
			return result
		except FileNotFoundError:
			continue
		except subprocess.TimeoutExpired:
			return subprocess.CompletedProcess([], 1, "", "Lock-freedom test timed out after 8 seconds")
		except Exception as e:
			continue
	
	return subprocess.CompletedProcess([], 1, "", "Lock-freedom test: Could not execute test")


def _run_bench(phase: str, timeout_seconds: int = 15) -> subprocess.CompletedProcess:
    # Use Maven exec plugin to run the correct main with args, matching manual usage
    user_home = os.path.expanduser("~")
    mvn_paths = ["mvn", f"{user_home}\\apache-maven-3.9.6\\bin\\mvn.cmd", "C:\\Program Files\\apache-maven-3.9.4\\bin\\mvn.cmd"]

    # Route to DS-appropriate test class
    ds_type = _CURRENT_DS_TYPE  # set at start of _run_test_logic for both seq and conc phases
    if phase == "seq":
        if ds_type == "stack":
            main_class = "com.example.test.BenchmarkSequentialStack"
        elif ds_type == "queue":
            main_class = "com.example.test.BenchmarkSequentialQueue"
        else:
            main_class = "com.example.test.BenchmarkSequential"
    elif phase == "conc_victim":
        if ds_type == "queue":
            main_class = "com.example.test.QueueNonBlockingTest"
        elif ds_type == "stack":
            main_class = "com.example.test.StackNonBlockingTest"
        else:
            main_class = "com.example.test.NonBlockingTest"
    else:  # phase == "conc" — consistency / sanity
        if ds_type == "queue":
            main_class = "com.example.test.QueueConsistencyTest"
        elif ds_type == "stack":
            main_class = "com.example.test.StackConsistencyTest"
        else:
            main_class = "com.example.test.ConsistencyTest"
    
    # Mirror user's manual command for both phases: no exec.args; rely on defaults in BenchMark_* classes
    exec_args = None
    for mvn_path in mvn_paths:
        try:
            cmd = [
                mvn_path,
                "-q",
                "exec:java",
                f"-Dexec.mainClass={main_class}",
            ]
            if exec_args:
                cmd.append(f"-Dexec.args={exec_args}")
            cmd += ["-f", str(PROJECT_ROOT / "pom.xml")]
            # Log exact exec command to the per-prompt log for parity checks
            try:
                with open((Path.cwd() / "prompt_logs" / "exec_cmd.log"), "a", encoding="utf-8") as _f:
                    _f.write(f"PHASE={phase} CMD={' '.join(cmd)}\n")
            except Exception:
                pass
            try:
                result = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    cwd=str(PROJECT_ROOT),
                    timeout=timeout_seconds,
                )
                return result
            except subprocess.TimeoutExpired as e:
                # On Windows, subprocess.run(timeout) might not kill the entire process tree (Maven -> Java)
                # We force kill all java and mvn processes to ensure a clean state
                if os.name == 'nt':
                    subprocess.run(["taskkill", "/F", "/IM", "java.exe", "/T"], capture_output=True)
                    subprocess.run(["taskkill", "/F", "/IM", "mvn.exe", "/T"], capture_output=True)
                
                out = (e.stdout or "") + (e.stderr or "")
                return subprocess.CompletedProcess(
                    [],
                    1,
                    e.stdout or "",
                    f"Benchmark timed out after {timeout_seconds}s\n{out}",
                )
        except FileNotFoundError:
            continue
    return subprocess.CompletedProcess([], 1, "", "Maven not found in PATH or standard locations")


def _write_to_logs(log_file: str, message: str):
	"""Write message to both per-prompt log and continuous log"""
	continuous_log = "continuous_logs.txt"
	for log_path in [log_file, continuous_log]:
		try:
			with open(log_path, "a", encoding="utf-8") as f:
				f.write(message)
				f.flush()
		except Exception:
			pass  # Ignore errors writing to logs


def _section(log_file: str, title: str) -> None:
	_write_to_logs(log_file, "\n" + ("=" * 80) + "\n" + title + "\n" + ("=" * 80) + "\n")


def _llm_for_lock_syntax() -> ChatOpenAI:
	api_key = os.environ.get("NVIDIA_NIM_API_KEY")
	if not api_key:
		raise ValueError("NVIDIA_NIM_API_KEY environment variable is not set")
	model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
	return ChatOpenAI(
		base_url="https://integrate.api.nvidia.com/v1",
		api_key=api_key,
		model=model,
		temperature=0,
		max_tokens=50,
	)


def classify_lock_syntax(java_code: str, log_file: str) -> str:
	"""
	Classify code by syntax only: looks for lock-based constructs (locks/synchronized).
	Returns: 'lock-free' | 'lock-based' | 'unknown' | 'error'
	"""
	try:
		# Fast local heuristic first
		lower = (java_code or "").lower()
		lock_markers = [
			".lock()", ".unlock()", "reentrantlock", "stampedlock", "readwritelock",
			"synchronized", "wait(", "notify(", "notif yall(", "condition", "semaphore",
		]
		if any(m in lower for m in lock_markers):
			return "lock-based"

		# LLM classifier to catch less-obvious lock usage / confirm lock-free CAS patterns.
		llm = _llm_for_lock_syntax()
		prompt_txt = (
			"Decide if the following Java code is LOCK-FREE or LOCK-BASED by SYNTAX only.\n"
			"Rules:\n"
			"- If it uses synchronized, Lock/Condition APIs, semaphores, monitors, or blocking locks => LOCK-BASED.\n"
			"- If it uses only atomics/CAS (AtomicReference, AtomicMarkableReference, compareAndSet loops) and no locks => LOCK-FREE.\n"
			"- If unsure => UNKNOWN.\n"
			"Output exactly one token: LOCK-FREE or LOCK-BASED or UNKNOWN.\n\n"
			"CODE:\n" + (java_code or "")
		)
		escaped = prompt_txt.replace("{", "{{").replace("}", "}}")
		chain = ChatPromptTemplate.from_messages([
			("system", "You are a strict classifier. Output only one token."),
			("user", escaped),
		]) | llm | StrOutputParser()
		out = (chain.invoke({}) or "").strip().upper()
		if "LOCK-FREE" in out:
			return "lock-free"
		if "LOCK-BASED" in out:
			return "lock-based"
		return "unknown"
	except Exception as e:
		_write_to_logs(log_file, f"[LOCK-SYNTAX] ERROR: {e}\n")
		return "error"

def _run_test_logic(state: GraphState, phase: str) -> Dict[str, Any]:
	# Set global DS type at the start for ALL phases (seq and conc) so _run_bench routes correctly.
	global _CURRENT_DS_TYPE
	_ds_name_early = state.get("data_structure", "")
	if _ds_name_early in _QUEUE_DS:
		_CURRENT_DS_TYPE = "queue"
	elif _ds_name_early in _STACK_DS:
		_CURRENT_DS_TYPE = "stack"
	else:
		_CURRENT_DS_TYPE = "set"

	def _with_history(result: Dict[str, Any]) -> Dict[str, Any]:
		if phase != "conc":
			return result
		history = list(state.get("conc_attempt_history", []))
		attempt_no = state.get("conc_attempt_count", 1)
		
		# Find existing entry (created by verify_structural) or create new one
		entry = next((h for h in history if h.get("attempt") == attempt_no), None)
		if entry:
			entry.update({
				"compilation_status": result.get("compilation_status", entry.get("compilation_status", "none")),
				"sanity_status": result.get("sanity_status", entry.get("sanity_status", "none")),
				"lock_syntax_status": result.get("lock_syntax_status", entry.get("lock_syntax_status", "unknown")),
				"lock_freedom_status": result.get("lock_freedom_status", entry.get("lock_freedom_status", "none")),
				"test_result": result.get("test_result", "fail"),
				"error_message": result.get("error_message", entry.get("error_message", "")),
				"code": state.get("generated_code", entry.get("code", ""))
			})
		else:
			entry = {
				"attempt": attempt_no,
				"compilation_status": result.get("compilation_status", "none"),
				"sanity_status": result.get("sanity_status", "none"),
				"lock_syntax_status": result.get("lock_syntax_status", "unknown"),
				"lock_freedom_status": result.get("lock_freedom_status", "none"),
				"test_result": result.get("test_result", "fail"),
				"error_message": result.get("error_message", ""),
				"code": state.get("generated_code", "")
			}
			history.append(entry)
			
		result["conc_attempt_history"] = history
		result["conc_attempt_count"] = attempt_no
		return result
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	try:
		if not state.get("generated_code"):
			_section(log_file, f"[{phase.upper()}] TEST START")
			_write_to_logs(log_file, f"[{phase.upper()}] ERROR: No code generated to test.\n")
			return _with_history({
				"test_result": "fail",
				"error_message": "No code generated to test.",
				"compilation_status": "fail",
				"sanity_status": "none",
				"lock_syntax_status": "unknown",
				"lock_freedom_status": "none",
				"failure_stage": "compile",
			})

		_write_code(state["generated_code"], phase)
		_section(log_file, f"[{phase.upper()}] TEST START")
		_write_to_logs(log_file, f"[{phase.upper()}] Starting COMPILE...\n")

		# Hide sources that cause compile errors for this DS type / phase:
		# - seq phase: concurrent-dependent sources + wrong-DS sequential benchmarks
		# - conc phase: wrong-DS sequential benchmarks (SequentialDataStructure still
		#               implements the ADT from the seq phase)
		backups = []
		sources_to_hide = list(_wrong_ds_benchmarks(_CURRENT_DS_TYPE))
		if phase == "seq":
			sources_to_hide = list(CONC_DEPENDENT_SOURCES) + sources_to_hide
		for src in sources_to_hide:
			if src.exists():
				dst = src.with_suffix(".java.bak")
				try:
					if dst.exists():
						dst.unlink()
					src.rename(dst)
					backups.append((dst, src))
					_write_to_logs(log_file, f"[{phase.upper()}] Temporarily moved {src.name} to {dst.name}\n")
				except Exception as e:
					_write_to_logs(log_file, f"[{phase.upper()}] WARNING: Failed to move {src.name}: {e}\n")
		
		c = _compile(timeout_seconds=120)
		if c.returncode != 0:
			# --- Inline compile reprompt (one-time) ---
			compile_retry_used = state.get("compile_retry_used", False)
			if not compile_retry_used:
				compile_error = c.stderr or c.stdout or "<no compiler output>"
				original_failed_code = state.get("generated_code", "")
				_write_to_logs(log_file, "[COMPILE REPROMPT] First compilation failed.\n")

				fixed_code, updated_history = _compile_reprompt(
					original_failed_code, compile_error, phase, log_file,
					state.get("conversation_history", [])
				)
				if fixed_code:
					_write_code(fixed_code, phase)
					c = _compile(timeout_seconds=120)
					if c.returncode == 0:
						_write_to_logs(log_file, "[COMPILE REPROMPT] Recompilation SUCCEEDED after LLM fix.\n")
						state["generated_code"] = fixed_code
						state["compile_retry_used"] = True
						state["conversation_history"] = updated_history
						# Fall through to continue with sanity tests below
					else:
						_write_to_logs(log_file, "[COMPILE REPROMPT] Recompilation FAILED even after LLM fix.\n")
				else:
					_write_to_logs(log_file, "[COMPILE REPROMPT] LLM returned empty/invalid code. Skipping retry.\n")

			# If compile still fails after the reprompt attempt (or reprompt was already used)
			if c.returncode != 0:
				# Log per-attempt RESULT immediately
				message = "RESULT: FAILURE\n\nERROR LOG (compile):\n" + (c.stderr or c.stdout or "<no compiler output>") + "\n\n"
				_write_to_logs(log_file, message)
				# Restore concurrent file if we hid it
				for dst, src in backups:
					if dst.exists():
						try:
							dst.rename(src)
							_write_to_logs(log_file, f"[SEQ] Restored {src.name} after compile\n")
						except Exception as e:
							_write_to_logs(log_file, f"[SEQ] WARNING: Failed to restore {src.name}: {e}\n")
				return _with_history({
					"test_result": "fail",
					"error_message": c.stderr or c.stdout,
					"compilation_status": "fail",
					"sanity_status": "none",
					"lock_syntax_status": "unknown",
					"lock_freedom_status": "none",
					"failure_stage": "compile",
					"compile_retry_used": True,
				})

		# Shared defaults
		lock_freedom_status = "none"
		lock_syntax_status = "unknown"

		# --- Concurrent phase: compile -> sanity (original) -> inject victim -> sanity (victim); log both times ---
		if phase == "conc":
			log_file = state.get("log_file_path") or "continuous_logs.txt"
			original_code = state.get("generated_code", "")
			# _CURRENT_DS_TYPE already set at function start for all phases

			# 1) First sanity test on original (non-instrumented) code; time and log
			_section(log_file, "[CONC] SANITY TEST 1 (original code)")
			_write_to_logs(log_file, "[CONC] Running first sanity test on original concurrent code...\n")
			
			original_code_sanitized = _sanitize_code(original_code, "conc")
			state["first_sanity_code"] = original_code_sanitized
			
			b1 = _run_bench("conc", timeout_seconds=15)
			out1 = (b1.stdout or "") + (b1.stderr or "")
			_write_to_logs(log_file, "[CONC] Sanity test 1 (original) completed.\n")

			if b1.returncode != 0:
				message = "RESULT: FAILURE\n\nERROR LOG (sanity-1 original):\n" + (out1 or "<no exec output>") + "\n\n"
				_write_to_logs(log_file, message)
				return _with_history({
					"test_result": "fail",
					"error_message": out1,
					"compilation_status": "pass",
					"sanity_status": "fail",
					"lock_syntax_status": lock_syntax_status,
					"lock_freedom_status": "none",
					"failure_stage": "first_sanity",
					"first_sanity_code": state.get("first_sanity_code", ""),
					"victim_injected_code": state.get("victim_injected_code", ""),
				})

			passed1 = "Sanity Test Passed" in out1
			if not passed1:
				message = "[CONC] First sanity test (original) FAILED.\n"
				message += "ERROR LOG (sanity-1):\n" + (out1 or "<no output>") + "\n\n"
				_write_to_logs(log_file, message)
				return _with_history({
					"test_result": "fail",
					"error_message": out1,
					"compilation_status": "pass",
					"sanity_status": "fail",
					"lock_syntax_status": lock_syntax_status,
					"lock_freedom_status": "none",
					"failure_stage": "first_sanity",
				})

			_write_to_logs(log_file, "[CONC] First sanity test (original) PASSED.\n\n")

			# 2) Inject victim stall logic, recompile, run second sanity; time and log
			_section(log_file, "[CONC] SANITY TEST 2 (victim-instrumented)")
			_write_to_logs(log_file, "[CONC] Injecting victim stall logic and running second sanity test...\n")
			instrumented_code = _inject_victim_sleep(original_code, ds_type=_CURRENT_DS_TYPE)
			instrumented_code_sanitized = _sanitize_code(instrumented_code, "conc")
			state["victim_injected_code"] = instrumented_code_sanitized
			
			try:
				_write_code(instrumented_code, "conc")
				c2 = _compile(timeout_seconds=120)
			except Exception as e:
				c2 = subprocess.CompletedProcess([], 1, "", str(e))

			if c2.returncode != 0:
				lock_freedom_status = "error"
				message = (
					"[CONC] RESULT: FAILURE (instrumented compile)\n\n"
					"ERROR LOG (compile-2):\n" + (c2.stderr or c2.stdout or "<no compiler output>") + "\n\n"
				)
				_write_to_logs(log_file, message)
				# Restore original code to file for consistency
				_write_code(original_code, "conc")
				return _with_history({
					"test_result": "fail",
					"error_message": c2.stderr or c2.stdout,
					"compilation_status": "pass",
					"sanity_status": "fail",  # second sanity could not be run -> overall sanity fail
					"lock_syntax_status": lock_syntax_status,
					"lock_freedom_status": lock_freedom_status,
					"failure_stage": "second_sanity",
					"first_sanity_code": state.get("first_sanity_code", ""),
					"victim_injected_code": state.get("victim_injected_code", ""),
				})

			b2 = _run_bench("conc_victim", timeout_seconds=15)
			out2 = (b2.stdout or "") + (b2.stderr or "")
			_write_to_logs(log_file, "[CONC] Sanity test 2 (victim-instrumented) completed.\n")

			# Restore original code to file so repo state is consistent for structural_correctness / later steps
			_write_code(original_code, "conc")
			_compile(timeout_seconds=120)

			if b2.returncode != 0:
				lock_freedom_status = "error"
				message = "RESULT: FAILURE\n\nERROR LOG (victim-sanity exec):\n" + (out2 or "<no exec output>") + "\n\n"
				_write_to_logs(log_file, message)
				return _with_history({
					"test_result": "fail",
					"error_message": out2,
					"compilation_status": "pass",
					"sanity_status": "fail",
					"lock_syntax_status": lock_syntax_status,
					"lock_freedom_status": lock_freedom_status,
					"failure_stage": "second_sanity",
					"first_sanity_code": state.get("first_sanity_code", ""),
					"victim_injected_code": state.get("victim_injected_code", ""),
				})

			passed2 = "Sanity Test Passed" in out2
			lock_freedom_status = "lock-free" if passed2 else "lock-based"

			status2 = "SUCCESS" if passed2 else "FAILURE"
			message = "[CONC EXECUTION] Sanity 1 (original) completed, Sanity 2 (victim) completed\n"
			message += f"[CONC EXECUTION] Result (victim-sanity): {status2}\n"
			message += f"Lock-Freedom Status: {lock_freedom_status}\n"
			if not passed2:
				message += "ERROR LOG (sanity with victim):\n" + (out2 or "<no output>") + "\n"
			message += "--- End of conc attempt ---\n\n"
			_write_to_logs(log_file, message)

			# Generated code passes only if compilation + first sanity + second sanity all pass
			return _with_history({
				"test_result": "pass" if passed2 else "fail",
				"error_message": "" if passed2 else out2,
				"compilation_status": "pass",
				"sanity_status": "pass" if passed2 else "fail",
				"lock_syntax_status": lock_syntax_status,
				"lock_freedom_status": lock_freedom_status,
				"failure_stage": "none" if passed2 else "second_sanity",
				"first_sanity_code": state.get("first_sanity_code", ""),
				"victim_injected_code": state.get("victim_injected_code", ""),
			})

		# --- Sequential phase: compile -> sanity benchmark (no victim, no lock-freedom) ---
		_section(log_file, f"[{phase.upper()}] SANITY/BENCHMARK")
		_write_to_logs(log_file, f"[{phase.upper()}] Starting SANITY/BENCHMARK...\n")
		b = _run_bench(phase, timeout_seconds=15)
		if b.returncode != 0:
			message = "RESULT: FAILURE\n\nERROR LOG (exec):\n" + (b.stderr or b.stdout or "<no exec output>") + "\n\n"
			_write_to_logs(log_file, message)
			if conc_moved:
				for src, dst in [
					(conc_backup, CONCURRENT_DS_PATH),
					(bench_conc_backup, BENCH_CONC_PATH),
					(lf_backup, LOCK_FREEDOM_PATH),
				]:
					if src.exists():
						try:
							src.rename(dst)
							_write_to_logs(log_file, f"[SEQ] Restored {dst.name} after benchmark\n")
						except Exception as e:
							_write_to_logs(log_file, f"[SEQ] WARNING: Failed to restore {dst.name}: {e}\n")
			return _with_history({
				"test_result": "fail",
				"error_message": b.stderr or b.stdout,
				"compilation_status": "pass",
				"sanity_status": "fail",
				"lock_syntax_status": lock_syntax_status,
				"lock_freedom_status": lock_freedom_status,
				"first_sanity_code": state.get("first_sanity_code", ""),
				"victim_injected_code": state.get("victim_injected_code", ""),
			})

		out = b.stdout or ""
		passed = "Sanity Test Passed" in out
		log_file = state.get("log_file_path") or "continuous_logs.txt"
		status = "SUCCESS" if passed else "FAILURE"
		message = f"[{phase.upper()} EXECUTION] Result: {status}\n"
		if phase == "conc":
			message += f"Lock-Freedom Status: {lock_freedom_status}\n"
		if not passed:
			message += "ERROR LOG (sanity):\n" + (out or "<no output>") + "\n"
		message += f"--- End of {phase} attempt ---\n\n"
		_write_to_logs(log_file, message)
		# Restoring is now handled in the finally block

		return _with_history({
			"test_result": "pass" if passed else "fail",
			"error_message": "" if passed else out,
			"compilation_status": "pass",
			"sanity_status": "pass" if passed else "fail",
			"lock_syntax_status": lock_syntax_status,
			"lock_freedom_status": lock_freedom_status,
			"first_sanity_code": state.get("first_sanity_code", ""),
			"victim_injected_code": state.get("victim_injected_code", ""),
		})
	except Exception as e:
		log_file = state.get("log_file_path") or "continuous_logs.txt"
		message = "RESULT: FAILURE\n\nERROR LOG (exception):\n" + str(e) + "\n\n"
		_write_to_logs(log_file, message)

		# Classify unexpected exceptions by stage so routing and logging are consistent.
		comp_ok = state.get("compilation_status") == "pass"
		sanity_status = state.get("sanity_status", "none")
		if not comp_ok:
			stage = "compile"
			sanity = "none"
		elif sanity_status != "pass":
			# We never successfully completed the first sanity test.
			stage = "first_sanity"
			sanity = "fail"
		else:
			# Compile and first sanity passed; treat as second-sanity failure.
			stage = "second_sanity"
			sanity = "fail"

		return _with_history({
			"test_result": "fail",
			"error_message": str(e),
			"compilation_status": state.get("compilation_status", "fail"),
			"sanity_status": sanity,
			"lock_syntax_status": state.get("lock_syntax_status", "unknown"),
			"lock_freedom_status": "error",
			"failure_stage": stage,
			"first_sanity_code": state.get("first_sanity_code", ""),
			"victim_injected_code": state.get("victim_injected_code", ""),
		})
	finally:
		# Always restore sources
		for src, dst in backups:
			if src.exists():
				try:
					if dst.exists():
						dst.unlink()
					src.rename(dst)
					_write_to_logs(log_file, f"[SEQ] Restored {dst.name}\n")
				except Exception as e:
					_write_to_logs(log_file, f"[SEQ] WARNING: Failed to restore {dst.name}: {e}\n")


def run_concurrent_tests_with_code(state: GraphState, code: str) -> Dict[str, Any]:
	"""
	Run concurrent compile/lock-freedom/sanity tests inline (used after human feedback).
	We copy state so we don't mutate other fields unexpectedly, but we honor attempt counts/history.
	"""
	local_state = dict(state)
	local_state["generated_code"] = code
	local_state["phase"] = "conc"
	# Increment conc attempt counter for history purposes
	local_state["conc_attempt_count"] = state.get("conc_attempt_count", 0) + 1
	return _run_test_logic(local_state, "conc")


@traceable
def node_test_code_seq(state: GraphState) -> Dict[str, Any]:
    # Enforce sequential phase context if needed, though state carries it
    return _run_test_logic(state, "seq")


@traceable
def node_test_code_conc(state: GraphState) -> Dict[str, Any]:
    # Set global DS type for _run_bench routing BEFORE entering test logic
    global _CURRENT_DS_TYPE
    ds_name = state.get("data_structure", "")
    if ds_name in _QUEUE_DS:
        _CURRENT_DS_TYPE = "queue"
    elif ds_name in _STACK_DS:
        _CURRENT_DS_TYPE = "stack"
    else:
        _CURRENT_DS_TYPE = "set"
    return _run_test_logic(state, "conc")