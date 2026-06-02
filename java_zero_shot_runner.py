"""
zero_shot_runner.py — Zero-shot lock-free data structure pipeline runner.

PIPELINE FLOW (per sample):
  [Stage 1] Prompt → LLM Generation
  [Stage 2] Compilation
  [Stage 3] Consistency Test (sanity)
  [Stage 4] Non-Blocking Test (victim-inject + sanity 2)
  [Stage 5] Extended CodeBLEU (semantic correctness)

Continuous log (continuous_logs_zero_shot.txt):
  - Written ONLY by this runner (not by sub-nodes)
  - Per-sample log is merged into continuous log after each sample
  - NOTE: nodes/test_code.py also internally writes to its own 'continuous_logs.txt';
    we capture those via per-sample log and include them by merging.

Usage:
    python zero_shot_runner.py                                       # all DS, 5 runs
    python zero_shot_runner.py --ds linked_list --num_runs 3
    python zero_shot_runner.py --ds binary_search_tree --model google/gemini-2.0-flash-001
    python zero_shot_runner.py --no-llm-judge                        # skip LLM judge (faster)
"""
from __future__ import annotations

import os
import sys
import json
import shutil
import argparse
from datetime import datetime
from pathlib import Path
from typing import Dict, Any, Optional

PROJECT_ROOT = Path(os.path.dirname(os.path.abspath(__file__)))
if str(PROJECT_ROOT) not in sys.path:
    sys.path.insert(0, str(PROJECT_ROOT))

from java_zero_shot.workflow_zero_shot import build_zero_shot_graph
from java_zero_shot.state_zero_shot import ZeroShotState
from zero_shot.prompts import ZERO_SHOT_PROMPTS
from extended_codebleu import evaluate_file as _codebleu_evaluate

# ── Data structures to run  ───────────────────────────────────────────────────
# Edit this list to add, remove, or reorder data structures.
# Valid values: linked_list | binary_search_tree | quad_tree | skiplist | b_minus_tree
DS_LIST = [
    "linked_list",
    "binary_search_tree",
    "skiplist",
    # "b_minus_tree",
    "hash_table",
    "queue",
    "stack",
]

CONTINUOUS_LOG   = PROJECT_ROOT / "continuous_logs_zero_shot.txt"
LOGS_BASE_DIR    = PROJECT_ROOT / "logs_zero_shot"
GEN_CODE_DIR     = PROJECT_ROOT / "generated_code_zero_shot"
RESULTS_DIR      = PROJECT_ROOT / "results_zero_shot"
BENCHMARK_SUMMARY_JSON = RESULTS_DIR / "benchmark_summary.json"
BENCHMARK_SUMMARY_CSV  = RESULTS_DIR / "benchmark_summary.csv"

CSV_HEADERS = [
    "SampleID", "DataStructure", "SampleIdx",
    # Pipeline stage results
    "Stage2_Compilation", "Stage3_Consistency", "Stage4_LockFreedom_Semantic",
    "Stage4_LockFreedom_Syntax", "Pipeline_Status",
    # Layer A & B — multipliers (reported, not weighted)
    "CB_LayerA_Consistency",      # hard binary: 1.0 pass / 0.0 fail
    "CB_LayerB_NonBlocking",      # hard binary: 1.0 lock-free / 0.0 otherwise
    # Weighted layers C + D
    "CB_LayerC_MultiRefCB",       # max CodeBLEU over ground-truth pool
    "CB_LayerD1_Annotation",      # annotation criterion match
    "CB_LayerD2_Concurrency",     # CAS/Atomic primitives
    "CB_LayerD3_LLMJudge",        # LLM-as-judge
    "CB_LayerD4_StructuralPatterns",  # CAS-loop + import + vocab
    # Summary
    "CB_Combined", "CB_IsCorrectDS", "CB_LLMVerdict", "CB_LLMReason",
]


# ── Continuous-log writer (this runner owns the continuous log) ───────────────

def _clog(message: str) -> None:
    """Append to continuous_logs_zero_shot.txt only."""
    with open(CONTINUOUS_LOG, "a", encoding="utf-8") as f:
        f.write(message); f.flush()


def _clog_section(title: str) -> None:
    bar = "=" * 80
    _clog(f"\n{bar}\n{title}\n{bar}\n")


def _clog_subsection(title: str) -> None:
    bar = "-" * 70
    _clog(f"\n{bar}\n  {title}\n{bar}\n")


def _merge_sample_log_into_clog(log_path: str) -> None:
    """Append the full per-sample log content into the continuous log."""
    try:
        content = Path(log_path).read_text(encoding="utf-8", errors="ignore")
        _clog(content)
    except Exception:
        pass


# ── Env setup ─────────────────────────────────────────────────────────────────

def _load_env(model_override: Optional[str]) -> None:
    env_path = PROJECT_ROOT / ".env"
    if env_path.exists():
        for raw in env_path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line: continue
            k, v = line.split("=", 1)
            k = k.strip(); v = v.strip().strip('"').strip("'")
            if k and v and not os.environ.get(k): os.environ[k] = v
    if model_override:
        os.environ["NVIDIA_NIM_MODEL"] = model_override


# ── File / stage helpers ──────────────────────────────────────────────────────

def _save_stage(code: str, name: str, stages_dir: Path, log_path: str) -> None:
    if not code or not code.strip(): return
    stages_dir.mkdir(parents=True, exist_ok=True)
    out = stages_dir / name
    out.write_text(code, encoding="utf-8")
    with open(log_path, "a", encoding="utf-8") as f:
        f.write(f"[Stage] Saved {name} ({len(code)} chars)\n"); f.flush()



# Lazy import to avoid circular imports at module load time
def _get_inject_fn():
    from nodes.test_code import _inject_victim_sleep, _QUEUE_DS, _STACK_DS
    return _inject_victim_sleep, _QUEUE_DS, _STACK_DS


def _save_all_stages(final_state: ZeroShotState, sample_gen_dir: Path, log_path: str) -> None:
    stages_dir = sample_gen_dir / "stages"
    gen  = final_state.get("generated_code", "")
    san  = final_state.get("first_sanity_code", "") or gen
    fin  = final_state.get("final_code", "") or gen

    # ── Stage 4: victim-injected code ───────────────────────────────────────
    # Try 1: LangGraph propagated the key
    vic = final_state.get("victim_injected_code", "")

    # Try 2: If state key is empty or identical to gen (not propagated),
    # re-derive the injected code here so stage_4 always shows real injection.
    if (not vic or vic == gen) and gen:
        try:
            _inject_victim_sleep, _QUEUE_DS, _STACK_DS = _get_inject_fn()
            ds_name = final_state.get("data_structure", "")
            if ds_name in _QUEUE_DS:
                ds_type = "queue"
            elif ds_name in _STACK_DS:
                ds_type = "stack"
            else:
                ds_type = "set"
            vic = _inject_victim_sleep(gen, ds_type=ds_type)
            with open(log_path, "a", encoding="utf-8") as f:
                f.write(f"[Stage] victim code re-derived via _inject_victim_sleep (ds_type={ds_type})\n")
                f.flush()
        except Exception as e:
            vic = gen   # last-resort fallback
            with open(log_path, "a", encoding="utf-8") as f:
                f.write(f"[Stage] WARNING: could not derive victim code: {e}\n"); f.flush()

    with open(log_path, "a", encoding="utf-8") as f:
        f.write("\n" + "="*80 + "\n[STAGE CODE SNAPSHOTS]\n" + "="*80 + "\n")

    _save_stage(gen, "stage_1_generated.java",    stages_dir, log_path)
    _save_stage(gen, "stage_2_post_compile.java", stages_dir, log_path)
    _save_stage(san, "stage_3_sanity.java",       stages_dir, log_path)
    _save_stage(vic, "stage_4_victim.java",       stages_dir, log_path)
    _save_stage(fin, "stage_5_final.java",        stages_dir, log_path)



# ── Results CSV ───────────────────────────────────────────────────────────────

def _ensure_csv(p: Path) -> None:
    if not p.exists():
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(",".join(CSV_HEADERS) + "\n", encoding="utf-8")


def _append_row(p: Path, row: list) -> None:
    with open(p, "a", encoding="utf-8") as f:
        f.write(",".join(str(c).replace(",", ";").replace("\n", " ") for c in row) + "\n")


# ── Stats ─────────────────────────────────────────────────────────────────────

def _empty_stats() -> dict:
    return dict(
        samples=0, success=0, compile_pass=0, sanity_pass=0,
        lf_sem={"lock-free": 0, "lock-based": 0, "error": 0, "none": 0},
        lf_syn={"lock-free": 0, "lock-based": 0, "unknown": 0},
        cb_sum={
            "consistency":         0.0,
            "nonblocking":         0.0,
            "multi_ref_cb":        0.0,
            "annotation":          0.0,
            "concurrency":         0.0,
            "llm_judge":           0.0,
            "structural_patterns": 0.0,
            "combined":            0.0,
        },
        cb_correct=0,
        combined_scores=[],   # raw combined per-sample for avg computation
    )


def _update_stats(stats: dict, fs: ZeroShotState, cb: Dict[str, Any]) -> None:
    stats["samples"] += 1
    if fs.get("test_result") == "pass":        stats["success"]      += 1
    if fs.get("compilation_status") == "pass": stats["compile_pass"] += 1
    if fs.get("sanity_status") == "pass":      stats["sanity_pass"]  += 1
    stats["lf_sem"][fs.get("lock_freedom_status", "none")] = \
        stats["lf_sem"].get(fs.get("lock_freedom_status", "none"), 0) + 1
    stats["lf_syn"][fs.get("lock_syntax_status", "unknown")] = \
        stats["lf_syn"].get(fs.get("lock_syntax_status", "unknown"), 0) + 1
    key_map = {
        "consistency":         "consistency_score",
        "nonblocking":         "nonblocking_score",
        "multi_ref_cb":        "multi_ref_cb_score",
        "annotation":          "annotation_score",
        "concurrency":         "concurrency_score",
        "llm_judge":           "llm_judge_score",
        "structural_patterns": "structural_patterns_score",
        "combined":            "combined_score",
    }
    for k, result_key in key_map.items():
        stats["cb_sum"][k] += cb.get(result_key, 0.0)
    combined = cb.get("combined_score", 0.0)
    stats["combined_scores"].append(combined)
    if cb.get("is_correct_ds"): stats["cb_correct"] += 1


def _write_stats(path: Path, ds: str, stats: dict) -> None:
    n  = max(stats["samples"], 1)
    cb = stats["cb_sum"]
    sc = stats["combined_scores"]
    avg_combined = sum(sc) / len(sc) if sc else 0.0
    with open(path, "a", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write(f"DATA STRUCTURE : {ds}\n")
        f.write(f"Total Samples  : {stats['samples']}\n")
        f.write(f"Pipeline Pass  : {stats['success']}\n")
        f.write(f"Compile Pass   : {stats['compile_pass']}\n")
        f.write(f"Sanity Pass    : {stats['sanity_pass']}\n")
        lfs = stats["lf_sem"]; lfy = stats["lf_syn"]
        f.write(f"LF Semantic    : lock-free={lfs.get('lock-free',0)}  lock-based={lfs.get('lock-based',0)}  error={lfs.get('error',0)}\n")
        f.write(f"LF Syntax      : lock-free={lfy.get('lock-free',0)}  lock-based={lfy.get('lock-based',0)}  unknown={lfy.get('unknown',0)}\n")
        f.write(f"── Extended CodeBLEU Benchmark (averages over {n} samples) ──\n")
        f.write(f"  [MULTIPLIERS — not weighted, gate combined score]\n")
        f.write(f"  Layer A  Consistency    : {cb['consistency']/n:.4f}  avg pass-rate (binary)\n")
        f.write(f"  Layer B  NonBlocking    : {cb['nonblocking']/n:.4f}  avg pass-rate (binary)\n")
        f.write(f"  [WEIGHTED LAYERS]\n")
        f.write(f"  Layer C  MultiRefCB     : {cb['multi_ref_cb']/n:.4f}  (max over pool, w=0.35)\n")
        f.write(f"  Layer D1 Annotation     : {cb['annotation']/n:.4f}  (ground-truth JSON, w=0.20)\n")
        f.write(f"  Layer D2 Concurrency    : {cb['concurrency']/n:.4f}  (CAS/Atomic prims, w=0.15)\n")
        f.write(f"  Layer D3 LLMJudge       : {cb['llm_judge']/n:.4f}  (LLM-as-judge, w=0.15)\n")
        f.write(f"  Layer D4 StructPatterns : {cb['structural_patterns']/n:.4f}  (CAS-loop+import+vocab, w=0.15)\n")
        f.write(f"  ── Aggregate ──\n")
        f.write(f"  Avg Combined Score      : {avg_combined:.4f}  (across {n} samples)\n")
        f.write(f"  Correct DS              : {stats['cb_correct']}/{n}\n")
        f.write("=" * 80 + "\n\n")


# ── Initial state ─────────────────────────────────────────────────────────────

def _build_state(ds: str, run_idx: int, log_path: str, prompt: str) -> ZeroShotState:
    return {
        "ds_name": ds, "prompt_idx": 1, "run_idx": run_idx,
        "approach": "zero_shot", "data_structure": ds,
        "prompt_topic": ds, "prompt_name": ds,
        "original_prompt": prompt, "current_prompt": prompt,
        "generated_code": "", "sequential_code": "",
        "conversation_history": [],
        "compilation_status": "none", "sanity_status": "none",
        "lock_freedom_status": "none", "lock_syntax_status": "unknown",
        "test_result": "fail", "error_message": "",
        "conc_attempt_count": 0,
        "max_retries": 0,           # no graph-level retries in zero-shot
        "conc_attempt_history": [], "failure_stage": "none",
        "first_sanity_retry_used": True,   # skip in-node retry logic
        "second_sanity_retry_used": True,
        "compile_retry_used": True,        # skip compile-reprompt in test_code.py
        "phase": "conc",
        "log_file_path": log_path, "_last_logged_key": "",
        "final_logs": [],
        "asked_human": False, "last_human_feedback": "",
        "human_feedback_count": 0, "human_feedback_1": "", "human_feedback_2": "",
        "final_code": "",
        "structural_expected": [], "structural_detected": [],
        "structural_score": 0.0, "structural_verify_status": "none",
        "structural_retry_used": False,
        "ds_judge_status":  "none",
        "ds_judge_verdict": "",
        "ds_judge_reason":  "",
        "first_sanity_code": "", "victim_injected_code": "",

    }


# ── CodeBLEU runner ───────────────────────────────────────────────────────────

def _run_codebleu(
    java_file: Path, ds: str, log_path: str, use_llm: bool,
    final_state: Optional[ZeroShotState] = None,
    ds_judge_verdict: str = "",
    ds_judge_reason: str = "",
) -> Dict[str, Any]:
    """Run Extended CodeBLEU (multiplier + 4-layer benchmark) and log results."""
    with open(log_path, "a", encoding="utf-8") as f:
        f.write("\n" + "="*80 + "\n")
        f.write("[STAGE 5 — EXTENDED CODEBLEU — BENCHMARK SCORING]\n")
        f.write("="*80 + "\n")
        f.write(f"  File      : {java_file}\n")
        f.write(f"  DS        : {ds}\n")
        f.write(f"  LLM Judge : {'ON' if use_llm else 'OFF'}\n")
        f.write(f"  Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.flush()

    test_results = None
    if final_state is not None:
        test_results = {
            "sanity_status":       final_state.get("sanity_status",       "none"),
            "lock_freedom_status": final_state.get("lock_freedom_status", "none"),
            "lock_syntax_status":  final_state.get("lock_syntax_status",  "unknown"),
        }

    # Determine D3 score from the DS judge response (already obtained during pipeline)
    _d3_score, _d3_verdict, _d3_reason = 0.5, "SKIP", ""
    if ds_judge_verdict in ("YES", "NO"):
        _d3_score   = 1.0 if ds_judge_verdict == "YES" else 0.0
        _d3_verdict = ds_judge_verdict
        _d3_reason  = ds_judge_reason

    _empty_cb: Dict[str, Any] = {
        "consistency_score": 0.0, "consistency_detail": "",
        "nonblocking_score": 0.0, "nonblocking_detail": "",
        "multi_ref_cb_score": 0.0, "multi_ref_cb_detail": "",
        "annotation_score": 0.5, "annotation_detail": "",
        "concurrency_score": 0.0, "concurrency_detail": "",
        "llm_judge_score": _d3_score, "llm_judge_verdict": _d3_verdict, "llm_judge_reason": _d3_reason,
        "structural_patterns_score": 0.0, "structural_patterns_detail": "",
        "combined_score": 0.0, "is_correct_ds": False, "error": "",
    }

    if not java_file.exists():
        _empty_cb["error"] = "File not found"
        with open(log_path, "a", encoding="utf-8") as f:
            f.write("  [ERROR] Java file not found!\n"); f.flush()
        return _empty_cb

    try:
        result = _codebleu_evaluate(
            java_file, ds, use_llm_judge=False,   # never re-call LLM; we inject below
            approach="zero_shot", test_results=test_results,
        )
        # Inject DS judge result as D3 (overrides the SKIP/0.5 default)
        result["llm_judge_score"]   = _d3_score
        result["llm_judge_verdict"] = _d3_verdict
        result["llm_judge_reason"]  = _d3_reason
    except Exception as exc:
        _empty_cb["error"] = str(exc)
        _empty_cb["llm_judge_verdict"] = "ERROR"
        _empty_cb["llm_judge_reason"] = str(exc)
        result = _empty_cb

    sep     = "─" * 68
    correct = result.get("is_correct_ds", False)
    a_ok    = result.get("consistency_score", 0.0) == 1.0
    b_ok    = result.get("nonblocking_score", 0.0) == 1.0
    gate_msg = "GATE: OPEN (both multipliers passed)" if (a_ok and b_ok) else \
               "GATE: CLOSED (≥1 multiplier failed → combined=0.0)"

    with open(log_path, "a", encoding="utf-8") as f:
        f.write(f"  {sep}\n")
        f.write(f"  EXTENDED CODEBLEU BENCHMARK — {ds.upper()}\n")
        f.write(f"  {gate_msg}\n")
        f.write(f"  {sep}\n")
        f.write(f"  [MULTIPLIERS — gate the score, not weighted]\n")
        f.write(f"  Layer A  │ Consistency Test      : {'PASS ✓' if a_ok else 'FAIL ✗'}"
                f"  ({result.get('consistency_score',0.0):.1f})\n")
        f.write(f"           │ {result.get('consistency_detail', 'N/A')}\n\n")
        f.write(f"  Layer B  │ Non-Blocking Semantic : {'PASS ✓' if b_ok else 'FAIL ✗'}"
                f"  ({result.get('nonblocking_score',0.0):.1f})\n")
        f.write(f"           │ {result.get('nonblocking_detail', 'N/A')}\n\n")
        f.write(f"  [WEIGHTED LAYERS]\n")
        f.write(f"  Layer C  │ Multi-Ref CodeBLEU   : {result.get('multi_ref_cb_score', 0.0):>7.4f}  (max, w=0.35)\n")
        f.write(f"           │ {result.get('multi_ref_cb_detail', 'N/A')[:140]}\n\n")
        f.write(f"  Layer D1 │ Annotation Match      : {result.get('annotation_score', 0.5):>7.4f}  (w=0.20)\n")
        f.write(f"           │ {result.get('annotation_detail', 'N/A')}\n\n")
        f.write(f"  Layer D2 │ Concurrency Prims     : {result.get('concurrency_score', 0.0):>7.4f}  (w=0.15)\n")
        f.write(f"           │ {result.get('concurrency_detail', 'N/A')}\n\n")
        f.write(f"  Layer D3 │ LLM Judge             : {result.get('llm_judge_score', 0.5):>7.4f}  (w=0.15)\n")
        f.write(f"           │ Verdict: {result.get('llm_judge_verdict','SKIP')}  "
                f"Reason: {result.get('llm_judge_reason','')}\n\n")
        f.write(f"  Layer D4 │ Structural Patterns   : {result.get('structural_patterns_score',0.0):>7.4f}  (w=0.15)\n")
        f.write(f"           │ {result.get('structural_patterns_detail','N/A')}\n\n")
        f.write(f"  {sep}\n")
        f.write(f"  COMBINED SCORE                  : {result.get('combined_score', 0.0):>7.4f}\n")
        f.write(f"  IS CORRECT DATA STRUCTURE?      : {'YES ✓' if correct else 'NO ✗'}\n")
        if result.get("error"):
            f.write(f"  ERROR                           : {result['error']}\n")
        f.write(f"  {sep}\n\n")
        f.flush()

    return result


# ── Benchmark summary ────────────────────────────────────────────────────────

BENCHMARK_SUMMARY_CSV_HEADERS = [
    "timestamp", "model", "data_structure", "n_samples",
    "avg_combined_score",
    "avg_layer_a_consistency",    # fraction of samples with A=1.0
    "avg_layer_b_nonblocking",    # fraction of samples with B=1.0
    "avg_layer_c_multi_ref_cb",
    "avg_layer_d1_annotation",
    "avg_layer_d2_concurrency",
    "avg_layer_d3_llm_judge",
    "avg_layer_d4_struct_patterns",
    "n_correct_ds",
]


def _write_benchmark_summary(
    model_name: str,
    ds: str,
    stats: dict,
    n_samples: int,
) -> None:
    """
    After all samples for a DS complete, compute averages and append to:
      - benchmark_summary.json  (list of dicts, machine-readable)
      - benchmark_summary.csv   (human-readable table)
    Also logs the summary to the continuous log.
    """
    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    n  = max(n_samples, 1)
    cb = stats["cb_sum"]
    sc = stats.get("combined_scores", [])
    avg_combined = sum(sc) / len(sc) if sc else 0.0

    entry = {
        "timestamp":                datetime.now().isoformat(),
        "model":                    model_name,
        "data_structure":           ds,
        "n_samples":                n_samples,
        "avg_combined_score":       round(avg_combined, 4),
        "avg_layer_a_consistency":  round(cb["consistency"] / n, 4),
        "avg_layer_b_nonblocking":  round(cb["nonblocking"] / n, 4),
        "avg_layer_c_multi_ref_cb": round(cb["multi_ref_cb"] / n, 4),
        "avg_layer_d1_annotation":  round(cb["annotation"] / n, 4),
        "avg_layer_d2_concurrency": round(cb["concurrency"] / n, 4),
        "avg_layer_d3_llm_judge":   round(cb["llm_judge"] / n, 4),
        "avg_layer_d4_struct_patterns": round(cb["structural_patterns"] / n, 4),
        "n_correct_ds":             stats["cb_correct"],
    }

    # ── JSON (append to list) ─────────────────────────────────────────────────
    existing: list = []
    if BENCHMARK_SUMMARY_JSON.exists():
        try:
            existing = json.loads(BENCHMARK_SUMMARY_JSON.read_text(encoding="utf-8"))
        except Exception:
            existing = []
    existing.append(entry)
    BENCHMARK_SUMMARY_JSON.write_text(
        json.dumps(existing, indent=2), encoding="utf-8"
    )

    # ── CSV (append row) ──────────────────────────────────────────────────────
    write_header = not BENCHMARK_SUMMARY_CSV.exists()
    with open(BENCHMARK_SUMMARY_CSV, "a", encoding="utf-8") as f:
        if write_header:
            f.write(",".join(BENCHMARK_SUMMARY_CSV_HEADERS) + "\n")
        row = [str(entry[h]) for h in BENCHMARK_SUMMARY_CSV_HEADERS]
        f.write(",".join(row) + "\n")

    # ── Continuous log ────────────────────────────────────────────────────────
    _clog(
        f"\n{'='*80}\n"
        f"[BENCHMARK SUMMARY] model={model_name}  ds={ds}  n={n_samples}\n"
        f"  Avg Combined Score    : {entry['avg_combined_score']:.4f}\n"
        f"  Layer A Consistency   : {entry['avg_layer_a_consistency']:.4f}  (pass-rate)\n"
        f"  Layer B NonBlocking   : {entry['avg_layer_b_nonblocking']:.4f}  (pass-rate)\n"
        f"  Layer C MultiRefCB    : {entry['avg_layer_c_multi_ref_cb']:.4f}\n"
        f"  Layer D1 Annotation   : {entry['avg_layer_d1_annotation']:.4f}\n"
        f"  Layer D2 Concurrency  : {entry['avg_layer_d2_concurrency']:.4f}\n"
        f"  Layer D3 LLMJudge     : {entry['avg_layer_d3_llm_judge']:.4f}\n"
        f"  Layer D4 StructPat    : {entry['avg_layer_d4_struct_patterns']:.4f}\n"
        f"  Correct DS            : {entry['n_correct_ds']}/{n_samples}\n"
        f"  Written to            : {BENCHMARK_SUMMARY_JSON.name}  &  {BENCHMARK_SUMMARY_CSV.name}\n"
        f"{'='*80}\n"
    )
    print(f"    [Benchmark Summary] {model_name}/{ds}: avg_combined={avg_combined:.4f} "
          f"(n={n_samples}, correct={entry['n_correct_ds']})")


# ── Main ──────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Zero-Shot Lock-Free DS Generator + Extended CodeBLEU",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--ds", nargs="+", choices=DS_LIST, default=DS_LIST)
    parser.add_argument("--num_runs", type=int, default=10)
    parser.add_argument("--model", default=None)
    parser.add_argument("--no-llm-judge", action="store_true",
                        help="Skip LLM-as-judge (faster, offline).")
    args = parser.parse_args()

    _load_env(args.model)
    use_llm = not args.no_llm_judge
    model_name  = os.environ.get("NVIDIA_NIM_MODEL", "unknown_model")
    model_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")

    for d in [LOGS_BASE_DIR, GEN_CODE_DIR, RESULTS_DIR]:
        d.mkdir(parents=True, exist_ok=True)

    # Initialise continuous log
    with open(CONTINUOUS_LOG, "w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write("ZERO-SHOT PIPELINE STARTED\n")
        f.write(f"Timestamp  : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Model      : {model_name}\n")
        f.write(f"DS list    : {args.ds}\n")
        f.write(f"Num runs   : {args.num_runs}\n")
        f.write(f"LLM judge  : {'ON' if use_llm else 'OFF'}\n")
        f.write("=" * 80 + "\n\n")

    print("[ZeroShot] Building LangGraph…")
    app = build_zero_shot_graph()
    print("[ZeroShot] Graph compiled.\n")

    for ds in args.ds:
        _clog_section(f"DATA STRUCTURE: {ds.upper()}")

        ds_results_dir = RESULTS_DIR / model_clean / ds
        ds_results_dir.mkdir(parents=True, exist_ok=True)
        results_file = ds_results_dir / "results.csv"
        stats_file   = ds_results_dir / "stats.txt"
        ds_log_dir   = LOGS_BASE_DIR / ds
        ds_log_dir.mkdir(parents=True, exist_ok=True)

        _ensure_csv(results_file)
        stats = _empty_stats()

        for run_idx in range(1, args.num_runs + 1):
            print(f"\n  [{ds}] Sample {run_idx}/{args.num_runs}")

            log_path = str(ds_log_dir / f"prompt_1_run_{run_idx}.log")

            # Initialise per-sample log
            with open(log_path, "w", encoding="utf-8") as f:
                f.write("=" * 80 + "\n")
                f.write(f"ZERO-SHOT PIPELINE — {ds.upper()} — Sample {run_idx}/{args.num_runs}\n")
                f.write(f"Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                f.write(f"Model     : {model_name}\n")
                f.write("=" * 80 + "\n\n")

            # Banner in continuous log
            _clog(f"\n{'>'*80}\n")
            _clog(f"  SAMPLE {run_idx}/{args.num_runs} — {ds.upper()}\n")
            _clog(f"  Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            _clog(f"  Log file  : {log_path}\n")
            _clog(f"{'>'*80}\n\n")

            initial_state = _build_state(ds, run_idx, log_path, ZERO_SHOT_PROMPTS[ds])

            # ── STAGE 1-4: run workflow (gen → compile → consistency → non-blocking) ──
            _clog_subsection("STAGES 1–4 : Generation → Compile → Consistency → Non-Blocking")
            try:
                final_state = app.invoke(initial_state)
            except Exception as exc:
                msg = f"[ERROR] Workflow failed: {exc}\n"
                print(f"    {msg}")
                with open(log_path, "a", encoding="utf-8") as f:
                    f.write(msg)
                _merge_sample_log_into_clog(log_path)
                continue

            # ── Write stage result summary to per-sample log ──────────────────
            compile_r  = final_state.get("compilation_status",  "none")
            sanity_r   = final_state.get("sanity_status",       "none")
            lf_sem_r   = final_state.get("lock_freedom_status", "none")
            lf_syn_r   = final_state.get("lock_syntax_status",  "unknown")
            test_r     = final_state.get("test_result",         "fail")
            pipeline_s = "SUCCESS" if test_r == "pass" else "FAILURE"

            # ── Live stage results — written immediately to continuous log ──────
            ts_now = datetime.now().strftime("%H:%M:%S")
            _clog(
                f"  [{ts_now}] Stage 2 | {ds} | Compile        : {compile_r.upper()}\n"
                f"  [{ts_now}] Stage 3 | {ds} | Consistency    : {sanity_r.upper()}\n"
                f"  [{ts_now}] Stage 4 | {ds} | LF Semantic    : {lf_sem_r.upper()} (syntax: {lf_syn_r})\n"
                f"  [{ts_now}] Stages 1–4 finished → Pipeline={pipeline_s}\n"
            )

            with open(log_path, "a", encoding="utf-8") as f:
                f.write("\n" + "="*80 + "\n")
                f.write("[PIPELINE STAGES 1–4 SUMMARY]\n")
                f.write("="*80 + "\n")
                f.write(f"  Stage 1 — Generation        : DONE\n")
                f.write(f"  Stage 2 — Compilation       : {compile_r.upper()}\n")
                f.write(f"  Stage 3 — Consistency Test  : {sanity_r.upper()}\n")
                f.write(f"  Stage 4 — Non-Blocking Test : {lf_sem_r.upper()} (syntax: {lf_syn_r})\n")
                f.write(f"  Pipeline Result             : {pipeline_s}\n")
                if final_state.get("error_message"):
                    f.write(f"  Error                       : {final_state.get('error_message', '')[:200]}\n")
                f.write("="*80 + "\n\n")
                f.flush()

            # ── Save stage code snapshots ─────────────────────────────────────
            sample_gen_dir = (
                GEN_CODE_DIR / model_clean / ds / f"prompt_1_sample_{run_idx}"
            )
            sample_gen_dir.mkdir(parents=True, exist_ok=True)
            _save_all_stages(final_state, sample_gen_dir, log_path)

            # Ensure primary Java file exists for CodeBLEU
            primary_java = sample_gen_dir / "ConcurrentDataStructure.java"
            if not primary_java.exists():
                code = final_state.get("final_code") or final_state.get("generated_code", "")
                if code:
                    primary_java.write_text(code, encoding="utf-8")

            # ── STAGE 5: Extended CodeBLEU ────────────────────────────────────
            _clog_subsection("STAGE 5 : Extended CodeBLEU")
            _clog(f"  [{datetime.now().strftime('%H:%M:%S')}] Stage 5 | {ds} | Running Extended CodeBLEU…\n")
            cb = _run_codebleu(
                primary_java, ds, log_path, use_llm,
                final_state=final_state,
                ds_judge_verdict=final_state.get("ds_judge_verdict", ""),
                ds_judge_reason=final_state.get("ds_judge_reason", ""),
            )
            _clog(f"  [{datetime.now().strftime('%H:%M:%S')}] Stage 5 | {ds} | CodeBLEU done → combined={cb.get('combined_score', 0):.4f}\n")

            # ── Copy artefacts to results directory ───────────────────────────
            sample_result_dir = ds_results_dir / f"sample_{run_idx}"
            sample_result_dir.mkdir(parents=True, exist_ok=True)

            if os.path.exists(log_path):
                shutil.copy2(log_path, sample_result_dir / "sample_log.txt")
            if primary_java.exists():
                shutil.copy2(primary_java, sample_result_dir / "ConcurrentDataStructure.java")

            stages_src = sample_gen_dir / "stages"
            if stages_src.exists():
                stages_dst = sample_result_dir / "stages"
                stages_dst.mkdir(exist_ok=True)
                for sf in stages_src.iterdir():
                    shutil.copy2(sf, stages_dst / sf.name)

            (sample_result_dir / "codebleu_report.json").write_text(
                json.dumps(cb, indent=2, default=str), encoding="utf-8"
            )

            # ── Merge per-sample log into continuous log ──────────────────────
            _merge_sample_log_into_clog(log_path)

            # ── Continuous log summary line ───────────────────────────────────
            _clog(
                f"\n[SAMPLE RESULT] {ds}_sample_{run_idx}\n"
                f"  Stage 2 Compile         : {compile_r}\n"
                f"  Stage 3 Consistency     : {sanity_r}\n"
                f"  Stage 4 NonBlocking     : {lf_sem_r}  (syntax: {lf_syn_r})\n"
                f"  Pipeline Status         : {pipeline_s}\n"
                f"  ─── Extended CodeBLEU Benchmark ───\n"
                f"  Layer A  Consistency    : {cb.get('consistency_score', 0.0):.1f}  (multiplier)  [{cb.get('consistency_detail', '')}]\n"
                f"  Layer B  NonBlocking    : {cb.get('nonblocking_score', 0.0):.1f}  (multiplier)  [{cb.get('nonblocking_detail', '')}]\n"
                f"  Layer C  MultiRefCB     : {cb.get('multi_ref_cb_score', 0.0):.4f}  (max,w=0.35) [{cb.get('multi_ref_cb_detail', '')[:80]}]\n"
                f"  Layer D1 Annotation     : {cb.get('annotation_score', 0.5):.4f}  (w=0.20) [{cb.get('annotation_detail', '')[:60]}]\n"
                f"  Layer D2 Concurrency    : {cb.get('concurrency_score', 0.0):.4f}  (w=0.15) [{cb.get('concurrency_detail', '')[:60]}]\n"
                f"  Layer D3 LLMJudge       : {cb.get('llm_judge_score', 0.5):.4f}  (w=0.15) Verdict={cb.get('llm_judge_verdict', 'SKIP')}\n"
                f"  Layer D4 StructPatterns : {cb.get('structural_patterns_score', 0.0):.4f}  (w=0.15) [{cb.get('structural_patterns_detail', '')[:60]}]\n"
                f"  Combined Score          : {cb.get('combined_score', 0.0):.4f}  IsCorrectDS={cb.get('is_correct_ds', False)}\n"
                f"{'-'*80}\n"
            )

            a_ok = cb.get('consistency_score', 0.0) == 1.0
            b_ok = cb.get('nonblocking_score', 0.0) == 1.0
            gate = '✓OPEN' if (a_ok and b_ok) else '✗GATE'
            print(
                f"    compile={compile_r:<6}  sanity={sanity_r:<6}  "
                f"lf={lf_sem_r:<12}  → {pipeline_s}\n"
                f"    A={'✓' if a_ok else '✗'}  B={'✓' if b_ok else '✗'}  [{gate}]  "
                f"C={cb.get('multi_ref_cb_score',0):.3f}  "
                f"D1={cb.get('annotation_score',0.5):.3f}  "
                f"D2={cb.get('concurrency_score',0):.3f}  "
                f"D3={cb.get('llm_judge_score',0.5):.3f}  "
                f"D4={cb.get('structural_patterns_score',0):.3f}  "
                f"combined={cb.get('combined_score',0):.3f}  "
                f"correct={'YES' if cb.get('is_correct_ds') else 'NO'}"
            )

            # ── Append to results CSV ─────────────────────────────────────────
            row = [
                f"{ds}_sample_{run_idx}", ds, str(run_idx),
                compile_r, sanity_r, lf_sem_r, lf_syn_r, pipeline_s.lower(),
                f"{cb.get('consistency_score',         0.0):.4f}",
                f"{cb.get('nonblocking_score',          0.0):.4f}",
                f"{cb.get('multi_ref_cb_score',         0.0):.4f}",
                f"{cb.get('annotation_score',           0.5):.4f}",
                f"{cb.get('concurrency_score',          0.0):.4f}",
                f"{cb.get('llm_judge_score',            0.5):.4f}",
                f"{cb.get('structural_patterns_score',  0.0):.4f}",
                f"{cb.get('combined_score',             0.0):.4f}",
                str(cb.get("is_correct_ds", False)),
                cb.get("llm_judge_verdict", "SKIP"),
                cb.get("llm_judge_reason", ""),
            ]
            _append_row(results_file, row)
            _update_stats(stats, final_state, cb)

        # ── DS-level stats ──────────────────────────────────────────────────
        _write_stats(stats_file, ds, stats)

        # ── Benchmark summary (avg combined per DS per model) ────────────────
        _write_benchmark_summary(model_name, ds, stats, stats["samples"])

        _clog(f"\n[DONE] {ds} → results: {results_file}\n")
        try:
            shutil.copy2(CONTINUOUS_LOG, ds_results_dir / "continuous_logs_zero_shot.txt")
        except Exception:
            pass

    _clog(
        f"\n{'='*80}\nZERO-SHOT PIPELINE COMPLETED\n"
        f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
        f"{'='*80}\n"
    )
    print(f"\nContinuous log     : {CONTINUOUS_LOG}")
    print(f"Results dir        : {RESULTS_DIR}")
    print(f"Benchmark summary  : {BENCHMARK_SUMMARY_JSON}")
    print(f"Benchmark CSV      : {BENCHMARK_SUMMARY_CSV}")


if __name__ == "__main__":
    main()
