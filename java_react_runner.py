"""
runner.py — Translation Pipeline Runner (Sequential → Concurrent)
=================================================================

Pipeline flow per sample:
  [Stage 1] Prompt → LLM generates SEQUENTIAL implementation
  [Stage 2] Test sequential code (compile + basic correctness)
  [Stage 3] LLM translates sequential → lock-free CONCURRENT
  [Stage 3] Structural verification (regex/AST: CAS, AtomicReference, etc.)
  [Stage 4] Concurrent test: compile + ConsistencyTest + NonBlockingTest (victim-inject)
  [Stage 5] Extended CodeBLEU benchmark scoring (of concurrent code only)

Outputs (per DS per model):
  results/<model>/<ds>/results.csv          ← per-sample scores
  results/<model>/<ds>/stats.txt            ← aggregated stats
  results/<model>/<ds>/sample_N/            ← per-sample artefacts
    ├── sample_log.txt
    ├── ConcurrentDataStructure.java
    └── codebleu_report.json
  results/<model>/<ds>/continuous_logs.txt  ← snapshot of full run log
  results/benchmark_summary.json            ← avg per DS per model (appended)
  results/benchmark_summary.csv             ← same, CSV

Usage:
  python runner.py
  python runner.py --prompts_dir prompts
"""
from __future__ import annotations

import os
import sys
import json
import shutil
import argparse
import glob
import re
from datetime import datetime
from pathlib import Path
from typing import List, Tuple, Dict, Any

import pandas as pd

from java_react_workflow import build_graph, GraphState
from extended_codebleu import evaluate_file as _codebleu_evaluate

PROJECT_ROOT = Path(os.path.dirname(os.path.abspath(__file__)))

# ── Constants ─────────────────────────────────────────────────────────────────

CONTINUOUS_LOG   = PROJECT_ROOT / "continuous_logs.txt"
LOGS_BASE_DIR    = PROJECT_ROOT / "logs"
GEN_CODE_DIR     = PROJECT_ROOT / "generated_code"
RESULTS_BASE_DIR = PROJECT_ROOT / "results"

BENCHMARK_SUMMARY_JSON = RESULTS_BASE_DIR / "benchmark_summary.json"
BENCHMARK_SUMMARY_CSV  = RESULTS_BASE_DIR / "benchmark_summary.csv"

BENCHMARK_SUMMARY_CSV_HEADERS = [
    "timestamp", "model", "data_structure", "n_samples",
    "avg_combined_score",
    "avg_layer_a_consistency",
    "avg_layer_b_nonblocking",
    "avg_layer_c_multi_ref_cb",
    "avg_layer_d1_annotation",
    "avg_layer_d2_concurrency",
    "avg_layer_d3_llm_judge",
    "avg_layer_d4_struct_patterns",
    "n_correct_ds",
]

# CSV headers for results.csv — pipeline stages + Extended CodeBLEU layers
CSV_HEADERS = [
    "SampleID", "DataStructure", "PromptIdx", "SampleIdx",
    # Pipeline stage results (best/final attempt)
    "Stage1_Sequential",               # sequential pass/fail
    "Stage3_Structural",               # structural verify pass/fail
    "Stage4_Compilation",              # concurrent compilation
    "Stage4_Consistency",              # ConsistencyTest (sanity)
    "Stage4_LockFreedom_Semantic",     # semantic lock-freedom (victim-inject)
    "Stage4_LockFreedom_Syntax",       # syntax lock-freedom scan
    "Pipeline_Status",                 # success / failure
    # Extended CodeBLEU — Layers A and B (multipliers, not weighted)
    "CB_LayerA_Consistency",           # 1.0 / 0.0 — consistency multiplier
    "CB_LayerB_NonBlocking",           # 1.0 / 0.0 — non-blocking multiplier
    # Weighted layers C + D
    "CB_LayerC_MultiRefCB",            # max CodeBLEU vs ground-truth pool
    "CB_LayerD1_Annotation",           # annotation JSON criterion match
    "CB_LayerD2_Concurrency",          # CAS/Atomic primitives
    "CB_LayerD3_LLMJudge",             # LLM-as-judge
    "CB_LayerD4_StructuralPatterns",   # CAS-loop + import + vocab
    "CB_Combined",                     # weighted sum (0 if gate failed)
    "CB_IsCorrectDS",
    "CB_LLMVerdict",
    "CB_LLMReason",
]


# =============================================================================
# ── Continuous log helpers ────────────────────────────────────────────────────
# =============================================================================

def _clog(msg: str) -> None:
    with open(CONTINUOUS_LOG, "a", encoding="utf-8") as f:
        f.write(msg)
        f.flush()


def _clog_section(title: str) -> None:
    _clog(f"\n{'='*80}\n{title}\n{'='*80}\n\n")


def _clog_subsection(title: str) -> None:
    _clog(f"\n{'─'*60}\n  {title}\n{'─'*60}\n")


# =============================================================================
# ── CSV helpers ───────────────────────────────────────────────────────────────
# =============================================================================

def _ensure_csv(path: Path) -> None:
    if not path.exists():
        with open(path, "w", encoding="utf-8") as f:
            f.write(",".join(CSV_HEADERS) + "\n")


def _append_row(path: Path, row: List[str]) -> None:
    cleaned = [str(v).replace(",", ";").replace("\n", " ").replace("\r", "") for v in row]
    with open(path, "a", encoding="utf-8") as f:
        f.write(",".join(cleaned) + "\n")


# =============================================================================
# ── Stats helpers ─────────────────────────────────────────────────────────────
# =============================================================================

def _empty_stats() -> Dict[str, Any]:
    return dict(
        samples=0, final_success=0, seq_success=0,
        compile_pass=0, sanity_pass=0,
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
        combined_scores=[],
    )


def _update_stats(
    stats: Dict[str, Any],
    final_state: GraphState,
    cb: Dict[str, Any],
    sequential_success: bool,
) -> None:
    stats["samples"] += 1
    final_ok = final_state.get("test_result") == "pass"
    if final_ok:               stats["final_success"] += 1
    if sequential_success:     stats["seq_success"]   += 1
    if final_state.get("compilation_status") == "pass": stats["compile_pass"] += 1
    if final_state.get("sanity_status")      == "pass": stats["sanity_pass"]  += 1

    lfs = str(final_state.get("lock_freedom_status", "none"))
    lfy = str(final_state.get("lock_syntax_status",  "unknown"))
    stats["lf_sem"][lfs] = stats["lf_sem"].get(lfs, 0) + 1
    stats["lf_syn"][lfy] = stats["lf_syn"].get(lfy, 0) + 1

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
    for k, rk in key_map.items():
        stats["cb_sum"][k] += cb.get(rk, 0.0)
    stats["combined_scores"].append(cb.get("combined_score", 0.0))
    if cb.get("is_correct_ds"): stats["cb_correct"] += 1


def _write_stats(path: Path, ds: str, stats: Dict[str, Any]) -> None:
    n  = max(stats["samples"], 1)
    cb = stats["cb_sum"]
    sc = stats["combined_scores"]
    avg_combined = sum(sc) / len(sc) if sc else 0.0
    lfs = stats["lf_sem"]; lfy = stats["lf_syn"]
    with open(path, "a", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write(f"DATA STRUCTURE : {ds}\n")
        f.write(f"Total Samples  : {n}\n")
        f.write(f"Final Success  : {stats['final_success']}\n")
        f.write(f"Seq Success    : {stats['seq_success']}\n")
        f.write(f"Compile Pass   : {stats['compile_pass']}\n")
        f.write(f"Sanity Pass    : {stats['sanity_pass']}\n")
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


def _write_benchmark_summary(
    model_name: str, ds: str, stats: Dict[str, Any], n_samples: int,
) -> None:
    """Append per-DS-per-model avg to benchmark_summary.json and .csv."""
    RESULTS_BASE_DIR.mkdir(parents=True, exist_ok=True)
    n  = max(n_samples, 1)
    cb = stats["cb_sum"]
    sc = stats.get("combined_scores", [])
    avg_combined = sum(sc) / len(sc) if sc else 0.0

    entry = {
        "timestamp":                   datetime.now().isoformat(),
        "model":                       model_name,
        "data_structure":              ds,
        "n_samples":                   n_samples,
        "avg_combined_score":          round(avg_combined, 4),
        "avg_layer_a_consistency":     round(cb["consistency"] / n, 4),
        "avg_layer_b_nonblocking":     round(cb["nonblocking"] / n, 4),
        "avg_layer_c_multi_ref_cb":    round(cb["multi_ref_cb"] / n, 4),
        "avg_layer_d1_annotation":     round(cb["annotation"] / n, 4),
        "avg_layer_d2_concurrency":    round(cb["concurrency"] / n, 4),
        "avg_layer_d3_llm_judge":      round(cb["llm_judge"] / n, 4),
        "avg_layer_d4_struct_patterns": round(cb["structural_patterns"] / n, 4),
        "n_correct_ds":                stats["cb_correct"],
    }

    # JSON
    existing: list = []
    if BENCHMARK_SUMMARY_JSON.exists():
        try:
            existing = json.loads(BENCHMARK_SUMMARY_JSON.read_text(encoding="utf-8"))
        except Exception:
            existing = []
    existing.append(entry)
    BENCHMARK_SUMMARY_JSON.write_text(json.dumps(existing, indent=2), encoding="utf-8")

    # CSV
    write_header = not BENCHMARK_SUMMARY_CSV.exists()
    with open(BENCHMARK_SUMMARY_CSV, "a", encoding="utf-8") as f:
        if write_header:
            f.write(",".join(BENCHMARK_SUMMARY_CSV_HEADERS) + "\n")
        f.write(",".join(str(entry[h]) for h in BENCHMARK_SUMMARY_CSV_HEADERS) + "\n")

    # Continuous log
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
    print(f"  [Benchmark Summary] {model_name}/{ds}: avg_combined={avg_combined:.4f} "
          f"(n={n_samples}, correct={entry['n_correct_ds']})")


# =============================================================================
# ── Extended CodeBLEU runner (Stage 5) ───────────────────────────────────────
# =============================================================================

def _run_codebleu(
    java_file: Path,
    ds: str,
    log_path: str,
    use_llm: bool,
    final_state: GraphState,
) -> Dict[str, Any]:
    """Run Extended CodeBLEU on the CONCURRENT java file. Log to per-sample log."""
    with open(log_path, "a", encoding="utf-8") as f:
        f.write("\n" + "="*80 + "\n")
        f.write("[STAGE 5 — EXTENDED CODEBLEU — BENCHMARK SCORING (CONCURRENT CODE)]\n")
        f.write("="*80 + "\n")
        f.write(f"  File      : {java_file}\n")
        f.write(f"  DS        : {ds}\n")
        f.write(f"  LLM Judge : {'ON' if use_llm else 'OFF'}\n")
        f.write(f"  Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        f.flush()

    test_results = {
        "sanity_status":       final_state.get("sanity_status",       "none"),
        "lock_freedom_status": final_state.get("lock_freedom_status", "none"),
        "lock_syntax_status":  final_state.get("lock_syntax_status",  "unknown"),
    }

    _empty_cb: Dict[str, Any] = {
        "consistency_score": 0.0, "consistency_detail": "",
        "nonblocking_score": 0.0, "nonblocking_detail": "",
        "multi_ref_cb_score": 0.0, "multi_ref_cb_detail": "",
        "annotation_score": 0.5, "annotation_detail": "",
        "concurrency_score": 0.0, "concurrency_detail": "",
        "llm_judge_score": 0.5, "llm_judge_verdict": "SKIP", "llm_judge_reason": "",
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
            java_file, ds,
            use_llm_judge=use_llm,
            approach="translation",
            test_results=test_results,
        )
    except Exception as exc:
        _empty_cb["error"] = str(exc)
        _empty_cb["llm_judge_verdict"] = "ERROR"
        _empty_cb["llm_judge_reason"] = str(exc)
        result = _empty_cb

    sep     = "─" * 68
    correct = result.get("is_correct_ds", False)
    a_ok    = result.get("consistency_score", 0.0) == 1.0
    b_ok    = result.get("nonblocking_score", 0.0) == 1.0
    gate    = "GATE: OPEN (both multipliers passed)" if (a_ok and b_ok) else \
              "GATE: CLOSED (≥1 multiplier failed → combined=0.0)"

    with open(log_path, "a", encoding="utf-8") as f:
        f.write(f"  {sep}\n")
        f.write(f"  EXTENDED CODEBLEU BENCHMARK — {ds.upper()}\n")
        f.write(f"  {gate}\n")
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


# =============================================================================
# ── Prompt loader ─────────────────────────────────────────────────────────────
# =============================================================================

def load_prompts_any(path: str) -> List[Tuple[str, str]]:
    lower = path.lower()
    if lower.endswith(".csv"):
        try:
            df = pd.read_csv(path, encoding="utf-8-sig")
        except Exception:
            raw = Path(path).read_text(encoding="utf-8-sig")
            lines = [ln for ln in raw.splitlines() if ln.strip()]
            if lines:
                hdr = lines[0].strip().lower()
                if "name" in hdr and "text" in hdr:
                    lines = lines[1:]
            return [(ln.strip(), ln.strip()) for ln in lines]
    else:
        df = pd.read_excel(path)
    names = df["name"].dropna().astype(str).tolist()
    texts = df["text"].dropna().astype(str).tolist()
    return list(zip(names, texts))


# =============================================================================
# ── Main ──────────────────────────────────────────────────────────────────────
# =============================================================================

def main() -> None:
    parser = argparse.ArgumentParser(
        description="Translation Pipeline: Sequential → Concurrent + Extended CodeBLEU"
    )
    parser.add_argument("--prompts_dir", default="prompts")
    parser.add_argument("--num_runs",    type=int, default=10)
    parser.add_argument("--no-llm-judge", action="store_true",
                        help="Skip LLM-as-judge (faster, offline).")
    args = parser.parse_args()

    # Load .env
    env_path = PROJECT_ROOT / ".env"
    if env_path.exists():
        for raw in env_path.read_text(encoding="utf-8").splitlines():
            line = raw.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            k = k.strip(); v = v.strip().strip('"').strip("'")
            if k and v and not os.environ.get(k):
                os.environ[k] = v

    use_llm      = not args.no_llm_judge
    model_name   = "deepseek-ai/deepseek-v3.2"
    model_clean  = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")

    for d in [LOGS_BASE_DIR, GEN_CODE_DIR, RESULTS_BASE_DIR]:
        d.mkdir(parents=True, exist_ok=True)

    # Initialise continuous log
    with open(CONTINUOUS_LOG, "w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write("TRANSLATION PIPELINE STARTED\n")
        f.write(f"Timestamp  : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Model      : {model_name}\n")
        f.write(f"Prompts    : {args.prompts_dir}\n")
        f.write(f"Num runs   : {args.num_runs}\n")
        f.write(f"LLM judge  : {'ON' if use_llm else 'OFF'}\n")
        f.write("=" * 80 + "\n\n")

    # CSV files to process
    ORDERED_CSV_FILES = [
        "queue.csv",
        # "hash_table.csv",
        "stack.csv",
        # "linked_list.csv",
        # "binary_search_tree.csv",
        # "skiplist.csv",
        # "b_minus_tree",
    ]
    if ORDERED_CSV_FILES:
        csv_files = [os.path.join(args.prompts_dir, f) for f in ORDERED_CSV_FILES]
    else:
        csv_files = sorted(glob.glob(os.path.join(args.prompts_dir, "*.csv")))

    print("[Translation] Building LangGraph…")
    app = build_graph()
    print("[Translation] Graph compiled.\n")

    base_results_dir = RESULTS_BASE_DIR / model_clean

    for csv_path in csv_files:
        if not Path(csv_path).exists():
            print(f"[WARN] CSV not found: {csv_path}")
            continue

        prompt_entries = load_prompts_any(csv_path)
        ds_name = Path(csv_path).stem                            # e.g. "linked_list"

        _clog_section(f"DATA STRUCTURE: {ds_name.upper()}")

        ds_results_dir = base_results_dir / ds_name
        ds_results_dir.mkdir(parents=True, exist_ok=True)
        results_file = ds_results_dir / "results.csv"
        stats_file   = ds_results_dir / "stats.txt"
        ds_log_dir   = LOGS_BASE_DIR / ds_name
        ds_log_dir.mkdir(parents=True, exist_ok=True)

        _ensure_csv(results_file)
        stats = _empty_stats()

        print(f"\n[Translation] Processing: {ds_name}  ({len(prompt_entries)} prompts × {args.num_runs} runs)")

        for prompt_idx, (prompt_name, seq_prompt) in enumerate(prompt_entries, 1):
            _clog(f"\n--- Prompt {prompt_idx}/{len(prompt_entries)}: {prompt_name} ---\n\n")

            for run_idx in range(1, args.num_runs + 1):
                print(f"\n  [{ds_name}] Prompt {prompt_idx} | Sample {run_idx}/{args.num_runs}")

                log_filename = f"prompt_{prompt_idx}_run_{run_idx}.log"
                log_path     = str(ds_log_dir / log_filename)

                with open(log_path, "w", encoding="utf-8") as f:
                    f.write("=" * 80 + "\n")
                    f.write(f"TRANSLATION PIPELINE — {ds_name.upper()} — Prompt {prompt_idx} Sample {run_idx}\n")
                    f.write(f"Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"Model     : {model_name}\n")
                    f.write("=" * 80 + "\n\n")

                _clog(f"\n{'>'*80}\n")
                _clog(f"  SAMPLE {run_idx}/{args.num_runs} — {ds_name.upper()}  (Prompt {prompt_idx})\n")
                _clog(f"  Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                _clog(f"  Log file  : {log_path}\n")
                _clog(f"{'>'*80}\n\n")

                initial_state: GraphState = {
                    "prompt_topic":         prompt_name,
                    "original_prompt":      seq_prompt,
                    "current_prompt":       seq_prompt,
                    "generated_code":       "",
                    "test_result":          "fail",
                    "error_message":        "",
                    "seq_attempt_count":    0,
                    "conc_attempt_count":   0,
                    "max_retries":          2,
                    "final_logs":           [],
                    "prompt_name":          prompt_name,
                    "log_file_path":        log_path,
                    "_last_logged_key":     "",
                    "phase":                "seq",
                    "asked_human":          False,
                    "sequential_code":      "",
                    "concurrent_code":      "",
                    "compilation_status":   "none",
                    "sanity_status":        "none",
                    "lock_freedom_status":  "none",
                    "lock_syntax_status":   "unknown",
                    "last_human_feedback":  "",
                    "human_feedback_count": 0,
                    "data_structure":       ds_name,
                    "prompt_idx":           prompt_idx,
                    "run_idx":              run_idx,
                    "conc_attempt_history": [],
                    "human_feedback_1":     "",
                    "human_feedback_2":     "",
                    "final_code":           "",
                    "structural_expected":  [],
                    "structural_detected":  [],
                    "structural_score":     0.0,
                    "failure_stage":        "none",
                    "first_sanity_retry_used":  False,
                    "second_sanity_retry_used": False,
                    "compile_retry_used":        False,
                    "structural_retry_used":     False,
                    "structural_verify_status":  "none",
                    "conversation_history":      [],
                }

                # ── STAGES 1–4: LangGraph ────────────────────────────────────
                _clog_subsection("STAGES 1–4: Generation → Sequential Test → Translation → Structural Verify → Concurrent Test")
                try:
                    final_state = app.invoke(initial_state)
                except Exception as exc:
                    msg = f"[ERROR] Workflow failed: {exc}\n"
                    print(f"    {msg}")
                    with open(log_path, "a", encoding="utf-8") as f:
                        f.write(msg)
                    _clog(msg)
                    continue

                # Extract key results
                phase            = final_state.get("phase", "seq")
                test_result      = final_state.get("test_result", "fail")
                compile_r        = final_state.get("compilation_status", "none")
                sanity_r         = final_state.get("sanity_status", "none")
                lf_sem_r         = final_state.get("lock_freedom_status", "none")
                lf_syn_r         = final_state.get("lock_syntax_status", "unknown")
                struct_r         = final_state.get("structural_verify_status", "none")
                pipeline_s       = "success" if test_result == "pass" else "failure"
                seq_ok           = phase == "conc" or (phase == "seq" and test_result == "pass")

                with open(log_path, "a", encoding="utf-8") as f:
                    f.write("\n" + "="*80 + "\n")
                    f.write("[PIPELINE STAGES 1–4 SUMMARY]\n")
                    f.write("="*80 + "\n")
                    f.write(f"  Stage 1 — Sequential Gen   : DONE\n")
                    f.write(f"  Stage 2 — Sequential Test  : {'PASS' if seq_ok else 'FAIL'}\n")
                    f.write(f"  Stage 3 — Translation Gen  : {'DONE' if phase == 'conc' else 'SKIPPED'}\n")
                    f.write(f"  Stage 3 — Structural Verify: {struct_r.upper()}\n")
                    f.write(f"  Stage 4 — Compilation      : {compile_r.upper()}\n")
                    f.write(f"  Stage 4 — Consistency Test : {sanity_r.upper()}\n")
                    f.write(f"  Stage 4 — LF Semantic      : {lf_sem_r.upper()}\n")
                    f.write(f"  Stage 4 — LF Syntax        : {lf_syn_r}\n")
                    f.write(f"  Pipeline Result            : {pipeline_s.upper()}\n")
                    if final_state.get("error_message"):
                        f.write(f"  Error                      : {final_state.get('error_message','')[:200]}\n")
                    f.write("="*80 + "\n\n")
                    f.flush()

                # ── Save concurrent code snapshot ────────────────────────────
                sample_gen_dir = GEN_CODE_DIR / model_clean / ds_name / f"prompt_{prompt_idx}_sample_{run_idx}"
                sample_gen_dir.mkdir(parents=True, exist_ok=True)

                concurrent_code = (final_state.get("final_code") or
                                   final_state.get("generated_code") or "")
                primary_java = sample_gen_dir / "ConcurrentDataStructure.java"
                if concurrent_code:
                    primary_java.write_text(concurrent_code, encoding="utf-8")

                # ── STAGE 5: Extended CodeBLEU ───────────────────────────────
                _clog_subsection("STAGE 5 : Extended CodeBLEU (Concurrent Code Only)")
                cb = _run_codebleu(primary_java, ds_name, log_path, use_llm, final_state)

                # ── Copy artefacts to results directory ──────────────────────
                sample_result_dir = ds_results_dir / f"sample_{prompt_idx}_{run_idx}"
                sample_result_dir.mkdir(parents=True, exist_ok=True)

                if os.path.exists(log_path):
                    shutil.copy2(log_path, sample_result_dir / "sample_log.txt")
                if primary_java.exists():
                    shutil.copy2(primary_java, sample_result_dir / "ConcurrentDataStructure.java")
                (sample_result_dir / "codebleu_report.json").write_text(
                    json.dumps(cb, indent=2, default=str), encoding="utf-8"
                )

                # ── Continuous log sample summary ────────────────────────────
                a_ok = cb.get("consistency_score", 0.0) == 1.0
                b_ok = cb.get("nonblocking_score", 0.0) == 1.0
                gate = "OPEN" if (a_ok and b_ok) else "GATE"
                sample_id = f"{ds_name}_p{prompt_idx}_s{run_idx}"
                _clog(
                    f"\n[SAMPLE RESULT] {sample_id}\n"
                    f"  Stage 1 Sequential      : {'PASS' if seq_ok else 'FAIL'}\n"
                    f"  Stage 3 Structural      : {struct_r}\n"
                    f"  Stage 4 Compile         : {compile_r}\n"
                    f"  Stage 4 Consistency     : {sanity_r}\n"
                    f"  Stage 4 NonBlocking     : {lf_sem_r}  (syntax: {lf_syn_r})\n"
                    f"  Pipeline Status         : {pipeline_s}\n"
                    f"  ─── Extended CodeBLEU Benchmark ───\n"
                    f"  Layer A  Consistency    : {cb.get('consistency_score', 0.0):.1f}  (multiplier)\n"
                    f"  Layer B  NonBlocking    : {cb.get('nonblocking_score', 0.0):.1f}  (multiplier)\n"
                    f"  [{gate}] A+B gate\n"
                    f"  Layer C  MultiRefCB     : {cb.get('multi_ref_cb_score', 0.0):.4f}  (max,w=0.35)\n"
                    f"  Layer D1 Annotation     : {cb.get('annotation_score', 0.5):.4f}  (w=0.20)\n"
                    f"  Layer D2 Concurrency    : {cb.get('concurrency_score', 0.0):.4f}  (w=0.15)\n"
                    f"  Layer D3 LLMJudge       : {cb.get('llm_judge_score', 0.5):.4f}  (w=0.15)  Verdict={cb.get('llm_judge_verdict','SKIP')}\n"
                    f"  Layer D4 StructPatterns : {cb.get('structural_patterns_score', 0.0):.4f}  (w=0.15)\n"
                    f"  Combined Score          : {cb.get('combined_score', 0.0):.4f}  IsCorrectDS={cb.get('is_correct_ds', False)}\n"
                    f"{'-'*80}\n"
                )

                print(
                    f"    seq={'OK' if seq_ok else 'FAIL'}  struct={struct_r}  "
                    f"compile={compile_r}  sanity={sanity_r}  lf={lf_sem_r}  -> {pipeline_s}\n"
                    f"    A={'OK' if a_ok else 'X'}  B={'OK' if b_ok else 'X'}  [{gate}]  "
                    f"C={cb.get('multi_ref_cb_score',0):.3f}  "
                    f"D1={cb.get('annotation_score',0.5):.3f}  "
                    f"D2={cb.get('concurrency_score',0):.3f}  "
                    f"D3={cb.get('llm_judge_score',0.5):.3f}  "
                    f"D4={cb.get('structural_patterns_score',0):.3f}  "
                    f"combined={cb.get('combined_score',0):.3f}  "
                    f"correct={'YES' if cb.get('is_correct_ds') else 'NO'}"
                )

                # ── Append to results CSV ────────────────────────────────────
                row = [
                    sample_id, ds_name, str(prompt_idx), str(run_idx),
                    "pass" if seq_ok else "fail",
                    struct_r, compile_r, sanity_r, lf_sem_r, lf_syn_r,
                    pipeline_s,
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
                _update_stats(stats, final_state, cb, seq_ok)

        # ── DS-level stats ────────────────────────────────────────────────────
        _write_stats(stats_file, ds_name, stats)
        _write_benchmark_summary(model_name, ds_name, stats, stats["samples"])

        _clog(f"\n[DONE] {ds_name} → results: {results_file}\n")
        try:
            shutil.copy2(CONTINUOUS_LOG, ds_results_dir / "continuous_logs.txt")
        except Exception:
            pass

    _clog(
        f"\n{'='*80}\nTRANSLATION PIPELINE COMPLETED\n"
        f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n"
        f"{'='*80}\n"
    )
    print(f"\nContinuous log     : {CONTINUOUS_LOG}")
    print(f"Results dir        : {RESULTS_BASE_DIR}")
    print(f"Benchmark summary  : {BENCHMARK_SUMMARY_JSON}")
    print(f"Benchmark CSV      : {BENCHMARK_SUMMARY_CSV}")


if __name__ == "__main__":
    main()
