# NBAgents: LLM-enabled Workflows for Non-blocking Data Structures

## Abstract

Designing non-blocking (lock-free) concurrent data structures is notoriously difficult, requiring a deep understanding of hardware memory models, atomic operations (e.g., Compare-And-Swap), and thread synchronization mechanisms. **NBAgents** is an automated framework designed to evaluate and enhance the capabilities of Large Language Models (LLMs) in generating and translating sequential data structures into lock-free, non-blocking concurrent variants. 

This repository implements two distinct pipelines for both **Java** and **C++**:
1. **Zero-Shot Translation**: Directly translating sequential code to concurrent code in a single generation step.
2. **ReAct Workflow**: An iterative, agentic approach utilizing the ReAct (Reasoning and Acting) framework, equipped with specialized tools for code compilation, structural verification, and concurrent testing.

## Repository Structure

```text
NBAgents-LLM-enabled-Workflows-for-Non-blocking-Data-Structures/
├── cpp_concurrent_testing/   # C++ Concurrent test harnesses (Consistency & Victim-injection)
├── cpp_react/                # C++ ReAct agent implementation
├── cpp_zero_shot/            # C++ Zero-shot baseline implementation
├── java_concurrent_testing/  # Java Concurrent test harnesses (Maven-based)
├── java_react/               # Java ReAct agent implementation
├── java_zero_shot/           # Java Zero-shot baseline implementation
├── prompts/                  # Baseline prompts (CSV) for various data structures
├── references/               # Ground-truth implementations and structural annotations
├── tools/                    # ReAct agent tools (compilation, testing, storage, logging)
├── generated_codes/          # Organized C++ and Java generated codes (zero-shot and translation)
├── zero_shot_big_model/      # Outputs and analysis from large chat agents (zero-shot & in-context)
├── extended_codebleu.py      # Extended CodeBLEU multi-layer evaluation script
├── java_react_runner.py      # Main entry point for Java ReAct pipeline
├── java_zero_shot_runner.py  # Main entry point for Java Zero-shot pipeline
├── cpp_react_runner.py       # Main entry point for C++ ReAct pipeline
└── cpp_zero_shot_runner.py   # Main entry point for C++ Zero-shot pipeline
```

## Methodology & Pipeline Overview

The evaluation and generation pipeline executes across the following five stages per sample:

1. **Stage 1 (Sequential Generation):** 
   An LLM uses provided prompts to generate a standard, sequential version of a specified data structure (e.g., Binary Search Tree, Hash Table, Linked List).
2. **Stage 2 (Sequential Validation):** 
   The sequential code is compiled and structurally tested to ensure baseline functional correctness.
3. **Stage 3 (Concurrent Translation & Verification):** 
   The LLM translates the sequential implementation into a lock-free concurrent version. The framework applies structural verification using regex and AST analysis to verify the presence of non-blocking primitives (`CompareAndSwap`, `std::atomic`, `AtomicReference`, etc.).
4. **Stage 4 (Concurrent Testing & Semantic Verification):** 
   The concurrent code is subjected to rigorous testing:
   - **Consistency Testing:** Basic single-threaded and multi-threaded sanity checks.
   - **Non-Blocking Test (Victim-Inject):** A semantic verification step that forcibly suspends interacting threads (victims) to verify that the remaining active threads can still make system-wide progress without deadlocking.
5. **Stage 5 (Extended CodeBLEU Scoring):** 
   A rigorous multi-layered evaluation of the final concurrent code (see Evaluation Metrics).

## Evaluation Metrics (Extended CodeBLEU)

Because standard CodeBLEU focuses primarily on semantic match and basic n-gram overlap, NBAgents implements an **Extended CodeBLEU** metric (`extended_codebleu.py`) explicitly tailored for concurrent code. It consists of multiple weighted layers:

- **Layer A (Consistency Multiplier):** A binary multiplier (0 or 1) based on basic compilation and state consistency.
- **Layer B (Non-Blocking Multiplier):** A binary multiplier enforcing true lock-freedom semantic verification (via Victim-Injection tests).
- **Layer C (Multi-Ref CB):** Standard CodeBLEU structural matching against a pool of ground-truth non-blocking reference implementations.
- **Layer D1 (Annotation Matching):** Evaluates structural and logic paths based on JSON annotations (`references/*_annotation.json`).
- **Layer D2 (Concurrency Primitives):** Verifies correct usage of memory barriers, CAS loops, and atomic data types.
- **Layer D3 (LLM-as-a-Judge):** Heuristic analysis utilizing an LLM to assess thread-safety and ABA-problem mitigation.
- **Layer D4 (Structural Patterns):** Evaluates higher-order concurrency constructs.

## Supported Data Structures

The framework evaluates the translation workflows on several fundamental data structures:
- Binary Search Tree
- Hash Table
- Linked List
- Quad Tree
- Queue
- Skip List
- Stack

## Setup & Usage

### Prerequisites
- **Python 3.10+**
- **Java:** JDK 11+ and Maven (`mvn`)
- **C++:** GCC/Clang with C++17 support and CMake

### Installation
1. Clone the repository.
2. Install Python dependencies:
   ```bash
   pip install -r requirements.txt
   ```
3. Set up your environment variables for the LLM API keys in a `.env` file at the repository root.

### Running the Pipelines

You can execute any of the runners to start the evaluation. The runners output results, continuous logs, and statistical summaries into dynamically generated `results/` and `logs/` directories.

**Java Zero-Shot:**
```bash
python java_zero_shot_runner.py --prompts_dir prompts --num_runs 5
```

**Java ReAct Workflow:**
```bash
python java_react_runner.py --prompts_dir prompts --num_runs 5
```

**C++ Zero-Shot:**
```bash
python cpp_zero_shot_runner.py --prompts_dir prompts --num_runs 5
```

**C++ ReAct Workflow:**
```bash
python cpp_react_runner.py --prompts_dir prompts --num_runs 5
```

By default, the runners append benchmarking metrics to `results/benchmark_summary.json` and `results/benchmark_summary.csv`.

## Contributors
*Developed for research into LLM-enabled compiler and concurrency engineering.*
