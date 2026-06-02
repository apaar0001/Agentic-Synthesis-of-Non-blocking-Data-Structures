"""
Zero-shot prompts for C++ concurrent data structures.

These prompts instruct the LLM to generate lock-free C++17 implementations
directly from the data structure name — no sequential pseudocode is provided.
"""

# SetADT interface contract included in Set-based prompts
_SET_ADT_INTERFACE = """\
#pragma once

class SetADT {
public:
    virtual ~SetADT() = default;
    virtual bool contains(int key) = 0;
    virtual bool add(int key) = 0;
    virtual bool remove(int key) = 0;
};"""

# Lock-free C++ implementation requirements
_LOCKFREE_CPP_INSTRUCTIONS = """\
Lock-Free C++17 Implementation Requirements:

1. NO LOCKS: Do not use std::mutex, std::lock_guard, std::unique_lock, or any blocking primitives.

2. ATOMIC OPERATIONS: Use std::atomic<T*> for all shared pointers with CAS loops.
   - Use compare_exchange_strong() or compare_exchange_weak() for atomic updates
   - Example: node->next.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)

3. MARKED POINTERS: Implement marked pointer helpers for logical deletion:
   ```cpp
   static bool is_marked_ref(Node* p) {
       return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
   }
   static Node* get_unmarked_ref(Node* p) {
       return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
   }
   static Node* get_marked_ref(Node* p) {
       return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
   }
   ```

4. MEMORY ORDERING: Use explicit memory ordering for all atomic operations:
   - std::memory_order_acquire for loads that synchronize with stores
   - std::memory_order_release for stores that other threads will observe
   - std::memory_order_acq_rel for read-modify-write operations (CAS)
   - std::memory_order_relaxed for operations that don't need synchronization

5. LOGICAL DELETION: For remove(), first logically mark the node, then physically unlink:
   - Mark the node's next pointer using get_marked_ref()
   - CAS to set the marked pointer
   - Skip marked nodes during traversal
   - Never use a marked node as a predecessor for CAS

6. RAII MEMORY MANAGEMENT: Implement a proper destructor that:
   - Traverses the entire structure
   - Uses get_unmarked_ref() to handle marked pointers
   - Deletes all allocated nodes
   - Prevents memory leaks

7. SENTINEL NODES: Use sentinel nodes with INT_MIN and INT_MAX to simplify edge cases:
   - Include <climits> for INT_MIN and INT_MAX
   - Initialize head to point to sentinel nodes in constructor

8. CLASS NAME: The implementation class MUST be named exactly ConcurrentDataStructure.

9. SINGLE-FILE IMPLEMENTATION: All code must be in a single .hpp file:
   - Include #pragma once at the top
   - Include necessary headers: <atomic>, <climits>, <cstdint>
   - Include "../utils/SetADT.hpp"
   - Define the class and all methods inline

10. LINEARIZABILITY: Each operation must appear atomic at a single linearization point.
    - The linearization point is typically the successful CAS operation
    - Skip/help-remove marked nodes during traversal

11. LOCK-FREEDOM: System-wide progress must be guaranteed even if threads stall.

12. OUTPUT: Return ONLY the C++ code in a single .hpp file, no explanations, no markdown.

13. CONSTRUCTOR: Include a no-argument constructor that initializes the data structure.
"""

# ── Per-data-structure zero-shot prompts ──────────────────────────────────────

ZERO_SHOT_PROMPTS = {
    "linked_list": f"""\
Act as a concurrent data structure expert. Implement a lock-free singly linked list in C++17 that \
implements the following interface:

{_SET_ADT_INTERFACE}

The linked list stores integer keys in sorted order (ascending).

The Node class must have:
  - An integer val field
  - A std::atomic<Node*> next pointer for lock-free operations

{_LOCKFREE_CPP_INSTRUCTIONS}

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
```

Implement the complete C++ class ConcurrentDataStructure in a single .hpp file.
The class must inherit from SetADT and override all three methods: contains(), add(), and remove().
""",

    "skiplist": f"""\
Act as a concurrent data structure expert. Implement a lock-free skip list in C++17 that implements \
the following interface:

{_SET_ADT_INTERFACE}

The skip list stores integer keys in sorted order across multiple levels (e.g., MAX_LEVEL = 16).

The Node class must have:
  - An integer val field
  - A std::atomic<Node*> forward array for each level: std::atomic<Node*> forward[MAX_LEVEL]
  - A topLevel field indicating the node's height

{_LOCKFREE_CPP_INSTRUCTIONS}

Additional skip list requirements:
- Use a random level generator for new nodes (e.g., geometric distribution)
- Maintain sentinel nodes at all levels with INT_MIN and INT_MAX
- Use marked pointers for logical deletion at each level
- Traverse from top level down to find insertion/deletion points

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
```

Implement the complete C++ class ConcurrentDataStructure in a single .hpp file.
The class must inherit from SetADT and override all three methods: contains(), add(), and remove().
""",

    "bst": f"""\
Act as a concurrent data structure expert. Implement a lock-free binary search tree (BST) in C++17 \
that implements the following interface:

{_SET_ADT_INTERFACE}

The BST must maintain the standard BST invariant: left child key < parent key < right child key.

The Node class must have:
  - An integer val field
  - A std::atomic<Node*> left child pointer
  - A std::atomic<Node*> right child pointer

{_LOCKFREE_CPP_INSTRUCTIONS}

Additional BST requirements:
- Use marked pointers for logical deletion of nodes
- Implement a search helper that finds the parent and target node
- Handle concurrent insertions and deletions at the same location
- Use CAS on parent's child pointer to atomically link/unlink nodes
- Consider using internal nodes and leaf nodes pattern for better concurrency

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
```

Implement the complete C++ class ConcurrentDataStructure in a single .hpp file.
The class must inherit from SetADT and override all three methods: contains(), add(), and remove().
""",

    "hash_table": f"""\
Act as a concurrent data structure expert. Implement a lock-free hash table in C++17 that implements \
the following interface:

{_SET_ADT_INTERFACE}

Use a fixed-size bucket array with lock-free linked lists for collision resolution.

The implementation must have:
  - A fixed-size array of bucket heads: std::atomic<Node*> buckets[BUCKET_COUNT]
  - A hash function: size_t hash(int key) that maps keys to bucket indices
  - Each bucket is a lock-free sorted linked list

The Node class must have:
  - An integer val field
  - A std::atomic<Node*> next pointer

{_LOCKFREE_CPP_INSTRUCTIONS}

Additional hash table requirements:
- Use a simple hash function like: hash(key) = (key & 0x7FFFFFFF) % BUCKET_COUNT
- Each bucket is a sorted linked list (for consistency with other structures)
- Use marked pointers for logical deletion within each bucket
- Implement search/add/remove by first hashing to find the bucket, then operating on that list
- Use sentinel nodes (INT_MIN, INT_MAX) for each bucket to simplify edge cases

Required includes:
```cpp
#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
```

Implement the complete C++ class ConcurrentDataStructure in a single .hpp file.
The class must inherit from SetADT and override all three methods: contains(), add(), and remove().
""",
}

# System prompt for the zero-shot generator LLM
ZERO_SHOT_SYSTEM_PROMPT = (
    "You are a world-class expert in concurrent systems and lock-free data structures in C++. "
    "You specialize in implementing lock-free and wait-free data structures using std::atomic and CAS operations. "
    "When asked to implement a concurrent data structure, you produce complete, production-quality "
    "C++17 code with no explanations or extra text.\n"
    "- Use std::atomic<T*> with compare_exchange_strong/weak for all atomic updates\n"
    "- Implement marked pointer helpers for logical deletion\n"
    "- Use explicit memory ordering (acquire/release/acq_rel/relaxed)\n"
    "- Implement proper RAII destructors to prevent memory leaks\n"
    "- Avoid ALL locking primitives (std::mutex, std::lock_guard, etc.)\n"
    "- Ensure linearizability and lock-freedom\n"
    "- Handle all edge cases (empty structure, concurrent modifications)\n"
    "NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments. "
    "Output ONLY the C++ code in a single .hpp file."
)

# All data structures available for zero-shot generation
DS_LIST = list(ZERO_SHOT_PROMPTS.keys())
