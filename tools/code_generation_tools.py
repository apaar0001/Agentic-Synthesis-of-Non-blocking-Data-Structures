"""
Code generation tools using LLM.
"""

from typing import Dict, Any
import os
from pathlib import Path
from langchain_core.prompts import ChatPromptTemplate
from langchain_core.output_parsers import StrOutputParser
from langchain_openai import ChatOpenAI
from langchain_google_genai import ChatGoogleGenerativeAI


def _get_nvidia_nim_llm() -> ChatOpenAI:
    """Get NVIDIA NIM LLM instance."""
    api_key = os.environ.get("NVIDIA_NIM_API_KEY")
    if not api_key:
        raise ValueError("NVIDIA_NIM_API_KEY environment variable is not set")
    
    model = os.environ.get("NVIDIA_NIM_MODEL", "nvidia/llama-3.1-nemotron-ultra-253b-v1")
    temperature = float(os.environ.get("NVIDIA_NIM_TEMPERATURE", 0.2))
    max_tokens = int(os.environ.get("NVIDIA_NIM_MAX_TOKENS", 1000))
    
    return ChatOpenAI(
        base_url="https://integrate.api.nvidia.com/v1",
        api_key=api_key,
        model=model,
        temperature=temperature,
        max_tokens=max_tokens
    )


def _extract_java_code(generated_code: str) -> str:
    """Extract Java code from generated text, removing markdown formatting."""
    lines = generated_code.split('\n')
    java_lines = []
    in_java_block = False
    
    for line in lines:
        if line.strip().startswith('```java'):
            in_java_block = True
            continue
        elif line.strip() == '```' and in_java_block:
            break
        elif in_java_block:
            java_lines.append(line)
        elif not in_java_block and (line.strip().startswith('package ') or 
                                    line.strip().startswith('import ') or
                                    line.strip().startswith('public class ') or
                                    line.strip().startswith('class ') or
                                    line.strip().startswith('interface ')):
            java_lines.append(line)
            in_java_block = True
    
    return '\n'.join(java_lines)


def _validate_and_fix_java_code(java_code: str) -> str:
    """Validate and fix package/import statements for sequential code."""
    if not java_code.strip():
        return java_code
    
    lines = java_code.split('\n')
    fixed_lines = []
    
    # Ensure correct package statement
    package_found = False
    import_found = False
    
    for line in lines:
        if line.strip().startswith('package '):
            # Replace with correct package
            fixed_lines.append('package com.example.Sets;')
            package_found = True
        elif line.strip().startswith('import ') and 'SetADT' in line:
            # Keep existing SetADT import
            fixed_lines.append(line)
            import_found = True
        else:
            fixed_lines.append(line)
    
    # Add missing package if not found
    if not package_found:
        fixed_lines.insert(0, 'package com.example.Sets;')
    
    # Add missing import if not found
    if not import_found:
        # Insert after package statement
        for i, line in enumerate(fixed_lines):
            if line.strip().startswith('package '):
                fixed_lines.insert(i + 1, 'import com.example.utils.SetADT;')
                break
    
    return '\n'.join(fixed_lines)


def generate_sequential_code(prompt: str, benchmark_source: str = "") -> Dict[str, Any]:
    """
    Tool to generate sequential Java code using LLM.
    
    Args:
        prompt: The user prompt for code generation
        benchmark_source: Optional benchmark source code to include
        
    Returns:
        Dict with 'code' (raw output) and 'java_code' (extracted and validated)
    """
    try:
        llm = _get_nvidia_nim_llm()
        user_prompt = f"{prompt}\n\n"
        
        escaped = user_prompt.replace("{", "{{").replace("}", "}}")
        chat_prompt = ChatPromptTemplate.from_messages([("user", escaped)])
        
        chain = chat_prompt | llm | StrOutputParser()
        raw_code = chain.invoke({})
        
        java_code = _extract_java_code(raw_code)
        fixed_code = _validate_and_fix_java_code(java_code)
        
        return {
            "code": raw_code,
            "java_code": fixed_code,
            "success": True
        }
    except Exception as e:
        return {
            "code": "",
            "java_code": "",
            "success": False,
            "error": str(e)
        }


def generate_concurrent_code(
    sequential_code: str,
    prompt: str = "",
    benchmark_source: str = "",
    attempt: int = 1,
    error_feedback: str = ""
) -> Dict[str, Any]:
    """
    Tool to generate concurrent Java code using LLM.
    
    Args:
        sequential_code: The sequential implementation to convert
        prompt: Optional custom prompt
        benchmark_source: Benchmark source code
        attempt: Current attempt number
        error_feedback: Error feedback from previous attempts
        
    Returns:
        Dict with 'code' (raw output) and 'java_code' (extracted and validated)
    """
    try:
        llm = _get_nvidia_nim_llm()
        
        if attempt == 1:
            # First attempt: use theory-based prompt
            conc_theory = """Convert the following sequential set implementation into a lock-free concurrent version using CAS.
Output Requirement: Provide only the concurrent code implementation with the class name ConcurrentDataStructure. No explanations, no extra text.
Lock-Free Implementation Instructions
No Locks Allowed
Do not use synchronized blocks or ReentrantLock.

CAS-based Updates
Use compareAndSet operations to atomically insert or delete nodes.
Ensure retry loops handle contention when multiple threads attempt to modify the same pointer.

Logical vs Physical Deletion
For deletion, first logically mark a node as deleted (using AtomicMarkableReference).
Later, physically remove the node from the structure when it is safe to do so.

Linearizability Requirement
Every operation (insert, delete, search) must appear as if it takes effect at a single instant.
Readers must not return logically deleted nodes as valid.

Task
Transform the SequentialDataStructure above into a lock-free concurrent implementation in Java named ConcurrentDataStructure.
Maintain lock-freedom (progress is guaranteed even if one thread is stalled).
Ensure linearizability (operations must appear atomic and consistent).
Avoid deadlocks and starvation by relying solely on atomic CAS loops.
Add detailed comments in the code explaining retry logic, CAS failures, and how deletions are handled.
API Contract (SetADT Interface):
package com.example.utils;
/***
* A simple set interface working with integer keys.
*/
public interface SetADT {
	boolean add(int key);
	boolean remove(int key);
	boolean contains(int key);
} \n Ensure that the generated code implements the SetADT interface."""
            
            test_theory = "Ensure that the Java code passes the testing script as mentioned below. The output of the testing script must be Sanity Test Passed."
            user_prompt = (
                conc_theory + "\n\nSequential code to transform:\n" + sequential_code + "\n\n"
                + test_theory + "\n\n"
                + "Testing script (mandatorily pass the sanity test):\n" + benchmark_source + "\n"
            )
        else:
            # Retry: use error feedback
            user_prompt = prompt if prompt else error_feedback
        
        escaped = user_prompt.replace("{", "{{").replace("}", "}}")
        system_prompt = "You are a helpful assistant that generates Java code given the sequential implementation of the code"
        chat_prompt = ChatPromptTemplate.from_messages([
            ("system", system_prompt),
            ("user", escaped)
        ])
        
        chain = chat_prompt | llm | StrOutputParser()
        raw_code = chain.invoke({})
        
        java_code = _extract_java_code(raw_code)
        fixed_code = _validate_and_fix_concurrent_code(java_code)
        
        return {
            "code": raw_code,
            "java_code": fixed_code,
            "success": True
        }
    except Exception as e:
        return {
            "code": "",
            "java_code": "",
            "success": False,
            "error": str(e)
        }


def _validate_and_fix_concurrent_code(java_code: str) -> str:
    """Validate and fix concurrent Java code."""
    if not java_code.strip():
        return java_code
    
    lines = java_code.split('\n')
    fixed_lines = []
    
    # Ensure correct package statement
    package_found = False
    import_found = False
    
    for line in lines:
        if line.strip().startswith('package '):
            # Replace with correct package
            fixed_lines.append('package com.example.Sets;')
            package_found = True
        elif line.strip().startswith('import ') and 'SetADT' in line:
            # Keep existing SetADT import
            fixed_lines.append(line)
            import_found = True
        else:
            fixed_lines.append(line)
    
    # Add missing package if not found
    if not package_found:
        fixed_lines.insert(0, 'package com.example.Sets;')
    
    # Add missing import if not found
    if not import_found:
        # Insert after package statement
        for i, line in enumerate(fixed_lines):
            if line.strip().startswith('package '):
                fixed_lines.insert(i + 1, 'import com.example.utils.SetADT;')
                break
    
    return '\n'.join(fixed_lines)
