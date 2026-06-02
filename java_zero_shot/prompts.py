"""
Zero-shot prompts for each data structure.

These prompts instruct the LLM to generate a lock-free implementation
directly from the data structure name — no sequential pseudocode is provided.
"""

# SetADT interface contract included in Set-based prompts
_SET_ADT_INTERFACE = """\
package com.example.utils;
/**
 * A simple set interface working with integer keys.
 */
public interface SetADT {
    boolean add(int key);
    boolean remove(int key);
    boolean contains(int key);
}"""

# QueueADT interface contract
_QUEUE_ADT_INTERFACE = """\
package com.example.utils;
/**
 * A lock-free integer Queue interface.
 */
public interface QueueADT {
    void enqueue(int val);
    int dequeue();      // returns -1 if empty
    boolean isEmpty();
}"""

# StackADT interface contract
_STACK_ADT_INTERFACE = """\
package com.example.utils;
/**
 * A lock-free integer Stack interface.
 */
public interface StackADT {
    void push(int val);
    int pop();          // returns -1 if empty
    boolean isEmpty();
}"""

# Lock-free instructions — Set variant
_LOCKFREE_INSTRUCTIONS = """\
Lock-Free Implementation Requirements:
1. NO LOCKS: Do not use synchronized, ReentrantLock, Semaphore, or any blocking primitives.
2. CAS-BASED UPDATES: Use AtomicReference, AtomicMarkableReference, or AtomicInteger with compareAndSet loops.
3. LOGICAL DELETION: For remove(), first logically mark the node (AtomicMarkableReference), then physically unlink.
   IMPORTANT: When using AtomicMarkableReference, never use .get(null). Use .getReference() instead.
4. LINEARIZABILITY: Each operation (add/remove/contains) must appear atomic at a single linearization point.
   - Skip/help-remove any logically-deleted (marked) nodes during traversal.
   - Never use a marked node as a predecessor for a CAS update.
5. LOCK-FREEDOM: System-wide progress must be guaranteed even if one thread stalls indefinitely.
6. CLASS NAME: The implementation class MUST be named exactly ConcurrentDataStructure.
7. PACKAGE: Must be in package com.example.Sets; and import com.example.utils.SetADT;
8. LINEARIZATION POINT COMMENT: Immediately after the successful CAS/operation that logically deletes a node,
   add exactly this comment on its own line:
       // Node has been marked
   Place it ONLY on the success path of logical deletion. Do NOT place it on retry/failure paths.
9. OUTPUT: Return ONLY the Java code, no explanations, no markdown text outside code blocks.
10. The generated class must include a no-argument constructor that initializes the data structure. Ensure the constructor does not take any parameters."""

# Lock-free instructions — Queue variant
_LOCKFREE_QUEUE_INSTRUCTIONS = """\
Lock-Free Queue Implementation Requirements:
1. NO LOCKS: Do not use synchronized, ReentrantLock, Semaphore, or any blocking primitives.
2. ALGORITHM: Implement the Michael-Scott lock-free queue (or equivalent CAS-based FIFO queue).
   - Maintain an AtomicReference<Node> for head and tail.
   - enqueue(): CAS on tail.next to link the new node, then advance tail.
   - dequeue(): CAS on head to advance past the sentinel node; return -1 if empty.
3. SENTINEL NODE: Use a sentinel (dummy) head node so head and tail are never null.
4. LOCK-FREEDOM: System-wide progress guaranteed even if one thread stalls indefinitely.
5. CLASS NAME: The implementation class MUST be named exactly ConcurrentDataStructure.
6. PACKAGE: Must be in package com.example.Sets; and import com.example.utils.QueueADT;
7. VICTIM COMMENT: Inside dequeue(), immediately after the successful CAS that removes the head (before returning the value),
   add exactly this comment on its own line:
       // Dequeue victim point
   Place it ONLY on the success path (CAS succeeded, not empty). Do NOT place it on empty/retry paths.
8. OUTPUT: Return ONLY the Java code, no explanations, no markdown text outside code blocks.
9. The generated class must include a no-argument constructor that initializes the data structure. Ensure the constructor does not take any parameters."""

# Lock-free instructions — Stack variant
_LOCKFREE_STACK_INSTRUCTIONS = """\
Lock-Free Stack Implementation Requirements:
1. NO LOCKS: Do not use synchronized, ReentrantLock, Semaphore, or any blocking primitives.
2. ALGORITHM: Implement Treiber's lock-free stack (or equivalent CAS-based LIFO stack).
   - Maintain an AtomicReference<Node> for top.
   - push(): Create a new Node(val, currentTop), CAS top from currentTop to newNode; retry on failure.
   - pop(): Read currentTop; if null return -1; CAS top from currentTop to currentTop.next; retry on failure.
3. LOCK-FREEDOM: System-wide progress guaranteed even if one thread stalls indefinitely.
4. CLASS NAME: The implementation class MUST be named exactly ConcurrentDataStructure.
5. PACKAGE: Must be in package com.example.Sets; and import com.example.utils.StackADT;
6. VICTIM COMMENT: Inside pop(), immediately after the successful CAS that removes the top node (before returning the value),
   add exactly this comment on its own line:
       // Pop victim point
   Place it ONLY on the success path (CAS succeeded, not empty). Do NOT place it on empty/retry paths.
7. OUTPUT: Return ONLY the Java code, no explanations, no markdown text outside code blocks.
8. The generated class must include a no-argument constructor that initializes the data structure. Ensure the constructor does not take any parameters."""

# ── Per-data-structure zero-shot prompts ──────────────────────────────────────

ZERO_SHOT_PROMPTS = {
    "linked_list": f"""\
Act as a concurrent data structure expert. Implement a lock-free singly linked list in Java that \
implements the following interface:

{_SET_ADT_INTERFACE}

The linked list stores integer keys in sorted order (ascending).
The Node class must have:
  - An integer key field
  - A next pointer (use AtomicMarkableReference<Node> for lock-free deletion)

{_LOCKFREE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "binary_search_tree": f"""\
Act as a concurrent data structure expert. Implement a lock-free binary search tree (BST) in Java \
that implements the following interface:

{_SET_ADT_INTERFACE}

The BST must maintain the standard BST invariant: left child key < parent key < right child key.
The Node class must have:
  - An integer key field
  - A left child pointer
  - A right child pointer
  (Use AtomicReference or AtomicMarkableReference for lock-free node management.)

{_LOCKFREE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "skiplist": f"""\
Act as a concurrent data structure expert. Implement a lock-free skip list in Java that implements \
the following interface:

{_SET_ADT_INTERFACE}

The skip list stores integer keys in sorted order across multiple levels.
The Node class must have:
  - An integer key field
  - A forward pointer array (AtomicMarkableReference<Node>[] or AtomicReference<Node>[]) for each level
  - A level/height field

{_LOCKFREE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "b_minus_tree": f"""\
Act as a concurrent data structure expert. Implement a lock-free B-tree (B-minus tree) in Java \
that implements the following interface:

{_SET_ADT_INTERFACE}

The B-tree stores integer keys. Each node can hold multiple keys and multiple child pointers.
The Node class must have:
  - An integer array or list for keys
  - A child pointer array or list (AtomicReference<Node>[] or equivalent)
  - An isLeaf boolean field

{_LOCKFREE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "hash_table": f"""\
Act as a concurrent data structure expert. Implement a lock-free hash table in Java that implements \
the following interface:

{_SET_ADT_INTERFACE}

Use a split-ordered list (Shalev-Shavit) or any CAS-based hash table design.
The implementation must support integer keys and handle:
  - Dynamic resizing using CAS (if applicable)
  - Bucket chains using AtomicReference<Node>

{_LOCKFREE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "queue": f"""\
Act as a concurrent data structure expert. Implement a lock-free FIFO queue in Java that implements \
the following interface:

{_QUEUE_ADT_INTERFACE}

Use the Michael-Scott CAS-based queue algorithm:
  - Sentinel (dummy) head node; AtomicReference<Node> head and tail
  - enqueue(): CAS on tail.next, then advance tail
  - dequeue(): CAS on head, advance past sentinel; return -1 if empty
The Node class must have:
  - An integer val field
  - An AtomicReference<Node> next field

{_LOCKFREE_QUEUE_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",

    "stack": f"""\
Act as a concurrent data structure expert. Implement a lock-free LIFO stack in Java that implements \
the following interface:

{_STACK_ADT_INTERFACE}

Use Treiber's algorithm:
  - AtomicReference<Node> top (null = empty)
  - push(): CAS top from oldTop to newNode(val, oldTop); retry on failure
  - pop(): CAS top from oldTop to oldTop.next; retry on failure; return -1 if empty
The Node class must have:
  - An integer val field
  - A Node next field (plain reference; only top needs AtomicReference)

{_LOCKFREE_STACK_INSTRUCTIONS}

Implement the complete Java class ConcurrentDataStructure in package com.example.Sets.
""",
}

# System prompt for the zero-shot generator LLM
ZERO_SHOT_SYSTEM_PROMPT = (
    "You are a world-class expert in concurrent systems and non-blocking data structures. "
    "You specialize in implementing lock-free and wait-free data structures using CAS operations. "
    "When asked to implement a concurrent data structure, you produce complete, production-quality "
    "Java code with no explanations or extra text.\n"
    "- Use AtomicReference, AtomicMarkableReference, or AtomicInteger with compareAndSet loops\n"
    "- Avoid ALL locking primitives (synchronized, ReentrantLock, Semaphore, etc.)\n"
    "- Ensure linearizability and lock-freedom\n"
    "- Handle all edge cases (empty structure, concurrent modifications)\n"
    "NOTE (VERY IMPORTANT): Do not add any detailed explanations, extra text or long comments. "
    "Output ONLY the Java code."
)

# All data structures available for zero-shot generation
DS_LIST = list(ZERO_SHOT_PROMPTS.keys())
