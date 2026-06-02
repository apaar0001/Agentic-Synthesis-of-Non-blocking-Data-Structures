"""
cpp_react_runner.py — C++ ReAct Translation Pipeline Runner
==============================================================

Mirrors runner.py for Java — main entry point for running the C++ ReAct pipeline.

Pipeline flow per sample:
  [Stage 1] Prompt → LLM generates SEQUENTIAL C++ implementation
  [Stage 2] Test sequential code (compile)
  [Stage 3] LLM translates sequential → lock-free CONCURRENT C++
  [Stage 3] Structural verification (regex: CAS, std::atomic, etc.)
  [Stage 4] Concurrent test: compile + ConsistencyTest + NonBlockingTest (victim-inject)
  [Stage 5] Extended CodeBLEU benchmark scoring (of concurrent code only)

Usage:
  python cpp_react_runner.py
  python cpp_react_runner.py --prompts_dir prompts --num_runs 10
  python cpp_react_runner.py --ds linked_list --num_runs 1
"""
from __future__ import annotations

import os
import sys
import json
import shutil
import argparse
import glob
from datetime import datetime
from pathlib import Path
from typing import List, Tuple, Dict, Any

import pandas as pd

from cpp_react.workflow import build_cpp_react_graph
from cpp_react.state import CppReactState

PROJECT_ROOT = Path(os.path.dirname(os.path.abspath(__file__)))

# ── Constants ─────────────────────────────────────────────────────────────────

CONTINUOUS_LOG   = PROJECT_ROOT / "continuous_logs_cpp_react.txt"
LOGS_BASE_DIR    = PROJECT_ROOT / "logs_cpp_react"
GEN_CODE_DIR     = PROJECT_ROOT / "generated_code_cpp_react"
RESULTS_BASE_DIR = PROJECT_ROOT / "results_cpp_react"

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

CSV_HEADERS = [
    "SampleID", "DataStructure", "PromptIdx", "SampleIdx",
    "Stage1_Sequential",
    "Stage3_Structural",
    "Stage4_Compilation",
    "Stage4_Consistency",
    "Stage4_LockFreedom_Semantic",
    "Stage4_LockFreedom_Syntax",
    "Pipeline_Status",
    "CB_LayerA_Consistency",
    "CB_LayerB_NonBlocking",
    "CB_LayerC_MultiRefCB",
    "CB_LayerD1_Annotation",
    "CB_LayerD2_Concurrency",
    "CB_LayerD3_LLMJudge",
    "CB_LayerD4_StructuralPatterns",
    "CB_Combined",
    "CB_IsCorrectDS",
    "CB_LLMVerdict",
    "CB_LLMReason",
]


# =============================================================================
# ── Helpers ───────────────────────────────────────────────────────────────────
# =============================================================================

def _clog(msg: str) -> None:
    with open(CONTINUOUS_LOG, "a", encoding="utf-8") as f:
        f.write(msg)
        f.flush()


def _ensure_csv(path: Path) -> None:
    if not path.exists():
        with open(path, "w", encoding="utf-8") as f:
            f.write(",".join(CSV_HEADERS) + "\n")


def _append_row(path: Path, row: List[str]) -> None:
    cleaned = [str(v).replace(",", ";").replace("\n", " ").replace("\r", "") for v in row]
    with open(path, "a", encoding="utf-8") as f:
        f.write(",".join(cleaned) + "\n")


def _empty_stats() -> Dict[str, Any]:
    return dict(
        samples=0, final_success=0, seq_success=0,
        compile_pass=0, sanity_pass=0,
        lf_sem={"lock-free": 0, "lock-based": 0, "error": 0, "none": 0},
        lf_syn={"lock-free": 0, "lock-based": 0, "unknown": 0},
        cb_sum={
            "consistency": 0.0, "nonblocking": 0.0,
            "multi_ref_cb": 0.0, "annotation": 0.0,
            "concurrency": 0.0, "llm_judge": 0.0,
            "structural_patterns": 0.0, "combined": 0.0,
        },
        cb_correct=0,
        combined_scores=[],
    )


def _update_stats(stats, final_state, cb, seq_ok):
    stats["samples"] += 1
    final_ok = final_state.get("test_result") == "pass"
    if final_ok:      stats["final_success"] += 1
    if seq_ok:        stats["seq_success"] += 1
    if final_state.get("compilation_status") == "pass": stats["compile_pass"] += 1
    if final_state.get("consistency_status") == "pass": stats["sanity_pass"] += 1

    lfs = str(final_state.get("lock_freedom_status", "none"))
    lfy = str(final_state.get("lock_syntax_status", "unknown"))
    stats["lf_sem"][lfs] = stats["lf_sem"].get(lfs, 0) + 1
    stats["lf_syn"][lfy] = stats["lf_syn"].get(lfy, 0) + 1

    key_map = {
        "consistency": "consistency_score", "nonblocking": "nonblocking_score",
        "multi_ref_cb": "multi_ref_cb_score", "annotation": "annotation_score",
        "concurrency": "concurrency_score", "llm_judge": "llm_judge_score",
        "structural_patterns": "structural_patterns_score", "combined": "combined_score",
    }
    for k, rk in key_map.items():
        stats["cb_sum"][k] += cb.get(rk, 0.0)
    stats["combined_scores"].append(cb.get("combined_score", 0.0))
    if cb.get("is_correct_ds"):
        stats["cb_correct"] += 1


def _write_stats(path, ds, stats):
    n = max(stats["samples"], 1)
    cb = stats["cb_sum"]
    sc = stats["combined_scores"]
    avg_combined = sum(sc) / len(sc) if sc else 0.0
    lfs, lfy = stats["lf_sem"], stats["lf_syn"]
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
        f.write(f"── Extended CodeBLEU (avg over {n} samples) ──\n")
        f.write(f"  Layer A  Consistency    : {cb['consistency']/n:.4f}\n")
        f.write(f"  Layer B  NonBlocking    : {cb['nonblocking']/n:.4f}\n")
        f.write(f"  Layer C  MultiRefCB     : {cb['multi_ref_cb']/n:.4f}\n")
        f.write(f"  Layer D1 Annotation     : {cb['annotation']/n:.4f}\n")
        f.write(f"  Layer D2 Concurrency    : {cb['concurrency']/n:.4f}\n")
        f.write(f"  Layer D3 LLMJudge       : {cb['llm_judge']/n:.4f}\n")
        f.write(f"  Layer D4 StructPatterns : {cb['structural_patterns']/n:.4f}\n")
        f.write(f"  Avg Combined Score      : {avg_combined:.4f}\n")
        f.write(f"  Correct DS              : {stats['cb_correct']}/{n}\n")
        f.write("=" * 80 + "\n\n")


def _write_benchmark_summary(model_name, ds, stats, n_samples):
    RESULTS_BASE_DIR.mkdir(parents=True, exist_ok=True)
    n = max(n_samples, 1)
    cb = stats["cb_sum"]
    sc = stats.get("combined_scores", [])
    avg_combined = sum(sc) / len(sc) if sc else 0.0

    entry = {
        "timestamp": datetime.now().isoformat(),
        "model": model_name,
        "data_structure": ds,
        "n_samples": n_samples,
        "avg_combined_score": round(avg_combined, 4),
        "avg_layer_a_consistency": round(cb["consistency"] / n, 4),
        "avg_layer_b_nonblocking": round(cb["nonblocking"] / n, 4),
        "avg_layer_c_multi_ref_cb": round(cb["multi_ref_cb"] / n, 4),
        "avg_layer_d1_annotation": round(cb["annotation"] / n, 4),
        "avg_layer_d2_concurrency": round(cb["concurrency"] / n, 4),
        "avg_layer_d3_llm_judge": round(cb["llm_judge"] / n, 4),
        "avg_layer_d4_struct_patterns": round(cb["structural_patterns"] / n, 4),
        "n_correct_ds": stats["cb_correct"],
    }

    # JSON
    existing = []
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

    print(f"  [Benchmark Summary] {model_name}/{ds}: avg_combined={avg_combined:.4f} "
          f"(n={n_samples}, correct={entry['n_correct_ds']})")


# =============================================================================
# ── CodeBLEU runner (Stage 5) ────────────────────────────────────────────────
# =============================================================================

def _run_codebleu(hpp_path, ds, log_path, use_llm, final_state):
    """Run Extended CodeBLEU on the concurrent .hpp file."""
    from cpp_codebleu.cpp_extended_codebleu import evaluate_cpp_file

    with open(log_path, "a", encoding="utf-8") as f:
        f.write("\n" + "=" * 80 + "\n")
        f.write("[STAGE 5 — EXTENDED CODEBLEU — BENCHMARK SCORING (C++ CONCURRENT CODE)]\n")
        f.write("=" * 80 + "\n")
        f.write(f"  File      : {hpp_path}\n")
        f.write(f"  DS        : {ds}\n")
        f.write(f"  LLM Judge : {'ON' if use_llm else 'OFF'}\n\n")
        f.flush()

    test_results = {
        "consistency_status":  final_state.get("consistency_status", "none"),
        "lock_freedom_status": final_state.get("lock_freedom_status", "none"),
        "lock_syntax_status":  final_state.get("lock_syntax_status", "unknown"),
    }

    _empty_cb = {
        "consistency_score": 0.0, "consistency_detail": "",
        "nonblocking_score": 0.0, "nonblocking_detail": "",
        "multi_ref_cb_score": 0.0, "multi_ref_cb_detail": "",
        "annotation_score": 0.5, "annotation_detail": "",
        "concurrency_score": 0.0, "concurrency_detail": "",
        "llm_judge_score": 0.5, "llm_judge_verdict": "SKIP", "llm_judge_reason": "",
        "structural_patterns_score": 0.0, "structural_patterns_detail": "",
        "combined_score": 0.0, "is_correct_ds": False, "error": "",
    }

    if not hpp_path.exists():
        _empty_cb["error"] = "File not found"
        return _empty_cb

    try:
        result = evaluate_cpp_file(
            hpp_path, ds,
            use_llm_judge=use_llm,
            approach="translation",
            test_results=test_results,
        )
    except Exception as exc:
        _empty_cb["error"] = str(exc)
        result = _empty_cb

    # Log results
    a_ok = result.get("consistency_score", 0.0) == 1.0
    b_ok = result.get("nonblocking_score", 0.0) == 1.0
    gate = "OPEN" if (a_ok and b_ok) else "GATE"

    with open(log_path, "a", encoding="utf-8") as f:
        f.write(f"  Layer A  : {'PASS' if a_ok else 'FAIL'}\n")
        f.write(f"  Layer B  : {'PASS' if b_ok else 'FAIL'}\n")
        f.write(f"  [{gate}]\n")
        f.write(f"  Layer C  : {result.get('multi_ref_cb_score', 0.0):.4f}\n")
        f.write(f"  Layer D1 : {result.get('annotation_score', 0.5):.4f}\n")
        f.write(f"  Layer D2 : {result.get('concurrency_score', 0.0):.4f}\n")
        f.write(f"  Layer D3 : {result.get('llm_judge_score', 0.5):.4f}\n")
        f.write(f"  Layer D4 : {result.get('structural_patterns_score', 0.0):.4f}\n")
        f.write(f"  Combined : {result.get('combined_score', 0.0):.4f}\n")
        f.write(f"  Correct  : {result.get('is_correct_ds', False)}\n\n")
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
        description="C++ ReAct Translation Pipeline: Sequential → Concurrent + Extended CodeBLEU"
    )
    parser.add_argument("--prompts_dir", default="prompts")
    parser.add_argument("--num_runs", type=int, default=10)
    parser.add_argument("--no-llm-judge", action="store_true")
    parser.add_argument("--ds", type=str, default=None,
                        help="Single DS to run (e.g. linked_list)")
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

    use_llm = not args.no_llm_judge
    model_name = os.environ.get("NVIDIA_NIM_MODEL", "deepseek-ai/deepseek-v3.2")
    model_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")

    for d in [LOGS_BASE_DIR, GEN_CODE_DIR, RESULTS_BASE_DIR]:
        d.mkdir(parents=True, exist_ok=True)

    # Init continuous log
    with open(CONTINUOUS_LOG, "w", encoding="utf-8") as f:
        f.write("=" * 80 + "\n")
        f.write("C++ REACT TRANSLATION PIPELINE STARTED\n")
        f.write(f"Timestamp  : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"Model      : {model_name}\n")
        f.write(f"Prompts    : {args.prompts_dir}\n")
        f.write(f"Num runs   : {args.num_runs}\n")
        f.write(f"LLM judge  : {'ON' if use_llm else 'OFF'}\n")
        f.write("=" * 80 + "\n\n")

    # CSV files to process
    if args.ds:
        csv_files = [os.path.join(args.prompts_dir, f"{args.ds}.csv")]
    else:
        csv_files = sorted(glob.glob(os.path.join(args.prompts_dir, "*.csv")))

    print("[C++ ReAct] Building LangGraph…")
    app = build_cpp_react_graph()
    print("[C++ ReAct] Graph compiled.\n")

    base_results_dir = RESULTS_BASE_DIR / model_clean

    for csv_path in csv_files:
        if not Path(csv_path).exists():
            print(f"[WARN] CSV not found: {csv_path}")
            continue

        prompt_entries = load_prompts_any(csv_path)
        ds_name = Path(csv_path).stem

        ds_results_dir = base_results_dir / ds_name
        ds_results_dir.mkdir(parents=True, exist_ok=True)
        results_file = ds_results_dir / "results.csv"
        stats_file = ds_results_dir / "stats.txt"
        ds_log_dir = LOGS_BASE_DIR / ds_name
        ds_log_dir.mkdir(parents=True, exist_ok=True)

        _ensure_csv(results_file)
        stats = _empty_stats()

        print(f"\n[C++ ReAct] Processing: {ds_name} ({len(prompt_entries)} prompts × {args.num_runs} runs)")

        for prompt_idx, (prompt_name, seq_prompt) in enumerate(prompt_entries, 1):
            for run_idx in range(1, args.num_runs + 1):
                print(f"\n  [{ds_name}] Prompt {prompt_idx} | Sample {run_idx}/{args.num_runs}")

                log_filename = f"prompt_{prompt_idx}_run_{run_idx}.log"
                log_path = str(ds_log_dir / log_filename)

                with open(log_path, "w", encoding="utf-8") as f:
                    f.write("=" * 80 + "\n")
                    f.write(f"C++ REACT PIPELINE — {ds_name.upper()} — Prompt {prompt_idx} Sample {run_idx}\n")
                    f.write(f"Timestamp : {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
                    f.write(f"Model     : {model_name}\n")
                    f.write("=" * 80 + "\n\n")

                initial_state: CppReactState = {
                    "prompt_topic": prompt_name,
                    "original_prompt": seq_prompt,
                    "current_prompt": seq_prompt,
                    "generated_code": "",
                    "test_result": "fail",
                    "error_message": "",
                    "seq_attempt_count": 0,
                    "conc_attempt_count": 0,
                    "final_logs": [],
                    "log_file_path": log_path,
                    "_last_logged_key": "",
                    "phase": "seq",
                    "sequential_code": "",
                    "concurrent_code": "",
                    "compilation_status": "none",
                    "consistency_status": "none",
                    "lock_freedom_status": "none",
                    "lock_syntax_status": "unknown",
                    "data_structure": ds_name,
                    "prompt_idx": prompt_idx,
                    "run_idx": run_idx,
                    "final_code": "",
                    "structural_expected": [],
                    "structural_detected": [],
                    "structural_score": 0.0,
                    "failure_stage": "none",
                    "first_sanity_retry_used": False,
                    "second_sanity_retry_used": False,
                    "compile_retry_used": False,
                    "structural_retry_used": False,
                    "structural_verify_status": "none",
                    "conversation_history": [],
                }

                # ── STAGES 1–4: LangGraph workflow
                try:
                    final_state = app.invoke(initial_state)
                except Exception as exc:
                    msg = f"[ERROR] Workflow failed: {exc}\n"
                    print(f"    {msg}")
                    with open(log_path, "a", encoding="utf-8") as f:
                        f.write(msg)
                    _clog(msg)
                    continue

                # Extract results
                phase = final_state.get("phase", "seq")
                test_result = final_state.get("test_result", "fail")
                compile_r = final_state.get("compilation_status", "none")
                sanity_r = final_state.get("consistency_status", "none")
                lf_sem_r = final_state.get("lock_freedom_status", "none")
                lf_syn_r = final_state.get("lock_syntax_status", "unknown")
                struct_r = final_state.get("structural_verify_status", "none")
                pipeline_s = "success" if test_result == "pass" else "failure"
                seq_ok = phase == "conc" or (phase == "seq" and test_result == "pass")

                # Save code snapshot
                sample_gen_dir = GEN_CODE_DIR / model_clean / ds_name / f"prompt_{prompt_idx}_sample_{run_idx}"
                sample_gen_dir.mkdir(parents=True, exist_ok=True)

                concurrent_code = (final_state.get("final_code") or
                                   final_state.get("generated_code") or "")
                primary_hpp = sample_gen_dir / "ConcurrentDataStructure.hpp"
                if concurrent_code:
                    primary_hpp.write_text(concurrent_code, encoding="utf-8")

                # ── STAGE 5: Extended CodeBLEU
                cb = _run_codebleu(primary_hpp, ds_name, log_path, use_llm, final_state)

                # Copy artefacts to results
                sample_result_dir = ds_results_dir / f"sample_{prompt_idx}_{run_idx}"
                sample_result_dir.mkdir(parents=True, exist_ok=True)

                if os.path.exists(log_path):
                    shutil.copy2(log_path, sample_result_dir / "sample_log.txt")
                if primary_hpp.exists():
                    shutil.copy2(primary_hpp, sample_result_dir / "ConcurrentDataStructure.hpp")
                (sample_result_dir / "codebleu_report.json").write_text(
                    json.dumps(cb, indent=2, default=str), encoding="utf-8"
                )

                a_ok = cb.get("consistency_score", 0.0) == 1.0
                b_ok = cb.get("nonblocking_score", 0.0) == 1.0
                gate = "OPEN" if (a_ok and b_ok) else "GATE"

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

                # Append to results CSV
                sample_id = f"{ds_name}_p{prompt_idx}_s{run_idx}"
                row = [
                    sample_id, ds_name, str(prompt_idx), str(run_idx),
                    "pass" if seq_ok else "fail",
                    struct_r, compile_r, sanity_r, lf_sem_r, lf_syn_r,
                    pipeline_s,
                    f"{cb.get('consistency_score', 0.0):.4f}",
                    f"{cb.get('nonblocking_score', 0.0):.4f}",
                    f"{cb.get('multi_ref_cb_score', 0.0):.4f}",
                    f"{cb.get('annotation_score', 0.5):.4f}",
                    f"{cb.get('concurrency_score', 0.0):.4f}",
                    f"{cb.get('llm_judge_score', 0.5):.4f}",
                    f"{cb.get('structural_patterns_score', 0.0):.4f}",
                    f"{cb.get('combined_score', 0.0):.4f}",
                    str(cb.get("is_correct_ds", False)),
                    cb.get("llm_judge_verdict", "SKIP"),
                    cb.get("llm_judge_reason", ""),
                ]
                _append_row(results_file, row)
                _update_stats(stats, final_state, cb, seq_ok)

        # DS-level stats
        _write_stats(stats_file, ds_name, stats)
        _write_benchmark_summary(model_name, ds_name, stats, stats["samples"])
        _clog(f"\n[DONE] {ds_name} → results: {results_file}\n")

    _clog(
        f"\n{'='*80}\nC++ REACT TRANSLATION PIPELINE COMPLETED\n"
        f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n{'='*80}\n"
    )
    print(f"\nContinuous log     : {CONTINUOUS_LOG}")
    print(f"Results dir        : {RESULTS_BASE_DIR}")
    print(f"Benchmark summary  : {BENCHMARK_SUMMARY_JSON}")
    print(f"Benchmark CSV      : {BENCHMARK_SUMMARY_CSV}")


if __name__ == "__main__":
    main()
