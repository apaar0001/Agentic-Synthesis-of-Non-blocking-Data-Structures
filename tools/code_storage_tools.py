"""
Code storage tools for saving generated code to directory structure.
"""

import os
from pathlib import Path
from typing import Dict, Any


def save_generated_code(
    code: str,
    data_structure: str,
    prompt_idx: int,
    run_idx: int,
    phase: str
) -> Dict[str, Any]:
    """
    Tool to save generated code to generated_code/model_name/data_structure/prompt_number_sample_number/
    
    Args:
        code: The Java code to save
        data_structure: Name of the data structure
        prompt_idx: Prompt index
        run_idx: Run/sample index
        phase: "seq" for sequential or "conc" for concurrent
        
    Returns:
        Dict with 'success', 'path', and optional 'error'
    """
    try:
        model_name = os.environ.get("NVIDIA_NIM_MODEL", "unknown_model")
        model_name_clean = model_name.replace("/", "_").replace(":", "_").replace(" ", "_")
        
        # Create directory structure
        base_dir = (
            Path(__file__).resolve().parents[1] / 
            "generated_code" / 
            model_name_clean / 
            data_structure / 
            f"prompt_{prompt_idx}_sample_{run_idx}"
        )
        base_dir.mkdir(parents=True, exist_ok=True)
        
        # Save code with appropriate filename
        if phase == "seq":
            filename = "SequentialDataStructure.java"
        else:
            filename = "ConcurrentDataStructure.java"
        
        code_path = base_dir / filename
        with open(code_path, "w", encoding="utf-8") as f:
            f.write(code)
        
        return {
            "success": True,
            "path": str(code_path),
            "error": ""
        }
    except Exception as e:
        return {
            "success": False,
            "path": "",
            "error": str(e)
        }
