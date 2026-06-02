"""Debug script to test codebleu library directly."""

from codebleu import calc_codebleu
from pathlib import Path

# Load a simple reference
ref_file = Path("cpp_concurrent_testing/structures/LockFreeList.hpp")
if ref_file.exists():
    ref_code = ref_file.read_text(encoding="utf-8")
    
    # Test 1: Compare against itself (should be high)
    print("Test 1: Comparing LockFreeList against itself")
    try:
        result = calc_codebleu(
            references=[ref_code],
            predictions=[ref_code],
            lang="cpp",
            weights=(0.25, 0.25, 0.25, 0.25),
            tokenizer=None
        )
        print(f"Result: {result}")
        print(f"CodeBLEU score: {result.get('codebleu', 0.0)}")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
    
    # Test 2: Try with simple code
    print("\n\nTest 2: Comparing simple code against LockFreeList")
    simple_code = """
    #include <atomic>
    class Node {
        int value;
        std::atomic<Node*> next;
    };
    """
    try:
        result = calc_codebleu(
            references=[ref_code],
            predictions=[simple_code],
            lang="cpp",
            weights=(0.25, 0.25, 0.25, 0.25),
            tokenizer=None
        )
        print(f"Result: {result}")
        print(f"CodeBLEU score: {result.get('codebleu', 0.0)}")
    except Exception as e:
        print(f"Error: {e}")
        import traceback
        traceback.print_exc()
else:
    print(f"Reference file not found: {ref_file}")
