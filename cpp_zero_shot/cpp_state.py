"""
State schema for C++ Zero-Shot Pipeline.

This module defines the CPPZeroShotState TypedDict that is used by all workflow nodes
in the LangGraph pipeline. The state tracks identity, prompts, generated code, stage
snapshots, test results, errors, CodeBLEU scores, and logging paths.
"""

from typing import TypedDict


class CPPZeroShotState(TypedDict):
    """
    State schema for C++ zero-shot pipeline workflow.
    
    This TypedDict defines all fields used throughout the 5-stage pipeline:
    1. Generation (LLM)
    2. Compilation (clang++)
    3. Consistency Test
    4. Non-Blocking Test (victim injection)
    5. Extended CodeBLEU
    
    All fields are required and must be initialized before workflow execution.
    """
    
    # ========== Identity Fields ==========
    ds_name: str
    """Data structure name: 'linked_list', 'skiplist', 'bst', or 'hash_table'"""
    
    sample_idx: int
    """1-indexed sample number for this generation run"""
    
    model_name: str
    """LLM model identifier (e.g., 'gpt-4', 'claude-3-opus')"""
    
    # ========== Prompts ==========
    original_prompt: str
    """Initial zero-shot prompt sent to the LLM"""
    
    current_prompt: str
    """Current prompt (may be modified in iterative workflows, unused in zero-shot)"""
    
    # ========== Generated Code ==========
    generated_code: str
    """Current C++ code (.hpp content) being processed"""
    
    final_code: str
    """Final code after all stages complete"""
    
    # ========== Stage Snapshots ==========
    stage_1_code: str
    """Code snapshot after Stage 1: Initial generation"""
    
    stage_2_code: str
    """Code snapshot after Stage 2: Post-compilation"""
    
    stage_3_code: str
    """Code snapshot after Stage 3: Post-consistency test"""
    
    stage_4_code: str
    """Code snapshot after Stage 4: Victim-injected for non-blocking test"""
    
    stage_5_code: str
    """Code snapshot after Stage 5: Final (same as final_code)"""
    
    # ========== Test Results ==========
    compilation_status: str
    """Compilation result: 'pass', 'fail', or 'none'"""
    
    consistency_status: str
    """Consistency test result: 'pass', 'fail', or 'none'"""
    
    lock_freedom_status: str
    """Semantic lock-freedom status: 'lock-free', 'lock-based', 'error', or 'none'"""
    
    lock_syntax_status: str
    """Syntactic lock detection: 'lock-free', 'lock-based', or 'unknown'"""
    
    # ========== Errors ==========
    error_message: str
    """Compilation or test error messages (empty string if no errors)"""
    
    # ========== CodeBLEU Scores ==========
    layer_a_score: float
    """Layer A: Consistency test multiplier (1.0 if pass, 0.0 if fail)"""
    
    layer_b_score: float
    """Layer B: Non-blocking test multiplier (1.0 if lock-free, 0.0 otherwise)"""
    
    layer_c_score: float
    """Layer C: Multi-reference CodeBLEU score (max over ground truth pool)"""
    
    layer_d1_score: float
    """Layer D1: Annotation score (pattern matching against DS-specific criteria)"""
    
    layer_d2_score: float
    """Layer D2: Concurrency primitives score (std::atomic, CAS, no locks)"""
    
    layer_d3_score: float
    """Layer D3: LLM judge score (optional YES/NO verdict)"""
    
    layer_d4_score: float
    """Layer D4: Structural patterns score (CAS-loop, includes, identifiers)"""
    
    combined_score: float
    """Final combined score: (A * B) * weighted_sum(C, D1, D2, D3, D4)"""
    
    # ========== Logging ==========
    log_file_path: str
    """Path to per-sample log file (e.g., 'logs_cpp/linked_list_sample_1.txt')"""
    
    continuous_log_path: str
    """Path to continuous log file (e.g., 'continuous_logs_cpp.txt')"""
