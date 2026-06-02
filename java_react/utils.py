import os
from pathlib import Path
from .state import GraphState

def save_generated_code(code: str, state: GraphState, filename: str) -> None:
	"""Save generated code to generated_code/model_name/data_structure/prompt_N_sample_M/"""
	model_name = os.environ.get("NVIDIA_NIM_MODEL", "unknown_model")
	model_name_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")
	
	prompt_idx = state.get("prompt_idx", 0)
	run_idx = state.get("run_idx", 0)
	data_structure = state.get("data_structure", "unknown_ds")
	
	# Directory structure: generated_code/model_name/data_structure/prompt_N_sample_M
	base_dir = Path(__file__).resolve().parents[1] / "generated_code" / model_name_clean / data_structure / f"prompt_{prompt_idx}_sample_{run_idx}"
	base_dir.mkdir(parents=True, exist_ok=True)
	
	code_path = base_dir / filename
	with open(code_path, "w", encoding="utf-8") as f:
		f.write(code)
	
	# Log the save location
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	continuous_log = "continuous_logs.txt"
	for log_path in [log_file, continuous_log]:
		try:
			with open(log_path, "a", encoding="utf-8") as f:
				f.write(f"Saved code to: {code_path}\n")
				f.flush()
		except Exception:
			pass

def save_continuous_log(state: GraphState) -> None:
	"""Save a copy of the current continuous_logs.txt to the results directory."""
	model_name = os.environ.get("NVIDIA_NIM_MODEL", "unknown_model")
	model_name_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")
	
	prompt_idx = state.get("prompt_idx", 0)
	run_idx = state.get("run_idx", 0)
	data_structure = state.get("data_structure", "unknown_ds")
	
	# Directory structure: generated_code/model_name/data_structure/prompt_N_sample_M
	base_dir = Path(__file__).resolve().parents[1] / "generated_code" / model_name_clean / data_structure / f"prompt_{prompt_idx}_sample_{run_idx}"
	base_dir.mkdir(parents=True, exist_ok=True)
	
	log_file = state.get("log_file_path") or "continuous_logs.txt"
	if os.path.exists(log_file):
		import shutil
		try:
			shutil.copy2(log_file, base_dir / "sample_log.txt")
		except Exception:
			pass
