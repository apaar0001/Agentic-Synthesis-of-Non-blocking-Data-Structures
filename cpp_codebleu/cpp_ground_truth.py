"""
Ground truth loader for C++ concurrent data structures.

This module loads the 19 existing C++ structures from cpp_concurrent_testing/structures/
to use as ground truth for CodeBLEU evaluation.

Requirements: 4.10.1, 4.10.2, 4.10.5
"""

from pathlib import Path
from typing import List, Tuple, Dict
import logging

# Cache for loaded ground truth files
_ground_truth_cache: Dict[str, List[Tuple[str, str]]] = {}

# Mapping from DS names to structure categories
DS_TO_STRUCTURES = {
    "linked_list": [
        "LockFreeList",
        "LazyList",
        "SelfishList",
        "VersionedList"
    ],
    "skiplist": [
        "FraserSkipList",
        "NoHotspotSkipList",
        "RotatingSkipList",
        "NumaskSkipList"
    ],
    "bst": [
        "LockFreeBSTree",
        "AVLTree",
        "RBTree",
        "NewAVLTree",
        "SFTree"
    ],
    "hash_table": [
        "LockFreeHashTable"
    ]
}


def load_ground_truth(ds_name: str) -> List[Tuple[str, str]]:
    """
    Load ground truth C++ structures for a given data structure category.
    
    Args:
        ds_name: Data structure name ("linked_list", "skiplist", "bst", "hash_table")
        
    Returns:
        List of (filename, code) tuples containing the ground truth structures
        
    Raises:
        ValueError: If ds_name is not recognized
        FileNotFoundError: If structures directory or files are not found
    """
    # Check cache first
    if ds_name in _ground_truth_cache:
        logging.info(f"Loading ground truth for {ds_name} from cache")
        return _ground_truth_cache[ds_name]
    
    # Validate DS name
    if ds_name not in DS_TO_STRUCTURES:
        raise ValueError(
            f"Unknown data structure: {ds_name}. "
            f"Valid options: {list(DS_TO_STRUCTURES.keys())}"
        )
    
    # Get structure names for this DS
    structure_names = DS_TO_STRUCTURES[ds_name]
    
    # Locate structures directory
    structures_dir = Path(__file__).resolve().parent.parent / "cpp_concurrent_testing" / "structures"
    if not structures_dir.exists():
        raise FileNotFoundError(
            f"Structures directory not found: {structures_dir}"
        )
    
    # Load all .hpp files for this DS
    ground_truth = []
    for structure_name in structure_names:
        hpp_file = structures_dir / f"{structure_name}.hpp"
        
        if not hpp_file.exists():
            logging.warning(f"Structure file not found: {hpp_file}")
            continue
        
        try:
            with open(hpp_file, 'r', encoding='utf-8') as f:
                code = f.read()
            ground_truth.append((structure_name, code))
            logging.info(f"Loaded ground truth: {structure_name}")
        except Exception as e:
            logging.error(f"Error loading {hpp_file}: {e}")
            continue
    
    if not ground_truth:
        raise FileNotFoundError(
            f"No ground truth structures found for {ds_name}"
        )
    
    # Cache the results
    _ground_truth_cache[ds_name] = ground_truth
    
    logging.info(
        f"Loaded {len(ground_truth)} ground truth structures for {ds_name}"
    )
    
    return ground_truth


def get_available_ds_names() -> List[str]:
    """
    Get list of available data structure names.
    
    Returns:
        List of valid DS names
    """
    return list(DS_TO_STRUCTURES.keys())


def get_structure_names(ds_name: str) -> List[str]:
    """
    Get list of structure names for a given DS category.
    
    Args:
        ds_name: Data structure name
        
    Returns:
        List of structure names
        
    Raises:
        ValueError: If ds_name is not recognized
    """
    if ds_name not in DS_TO_STRUCTURES:
        raise ValueError(
            f"Unknown data structure: {ds_name}. "
            f"Valid options: {list(DS_TO_STRUCTURES.keys())}"
        )
    return DS_TO_STRUCTURES[ds_name]


def clear_cache():
    """Clear the ground truth cache."""
    global _ground_truth_cache
    _ground_truth_cache.clear()
    logging.info("Ground truth cache cleared")


if __name__ == "__main__":
    # Simple test
    logging.basicConfig(level=logging.INFO)
    
    print("Available DS names:", get_available_ds_names())
    print()
    
    for ds_name in get_available_ds_names():
        print(f"\n{ds_name}:")
        print(f"  Structure names: {get_structure_names(ds_name)}")
        
        try:
            ground_truth = load_ground_truth(ds_name)
            print(f"  Loaded {len(ground_truth)} structures")
            for filename, code in ground_truth:
                print(f"    - {filename}: {len(code)} characters")
        except Exception as e:
            print(f"  Error: {e}")
