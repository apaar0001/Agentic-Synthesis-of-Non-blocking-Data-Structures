class QueueADT {
public:
    virtual ~QueueADT() = default;
    virtual void enqueue(int val) = 0;
    virtual int dequeue() = 0;
    virtual bool isEmpty() = 0;
};

We need to implement class ConcurrentDataStructure : public QueueADT.

We need to include "../utils/QueueADT.hpp". We'll just include that.

Now we need to place #pragma once at top, includes.

Now we need to define the marked pointer helper functions as static inline inside the class or as private static methods.

We'll define them as private static inline functions.

Now we need to ensure we use correct memory ordering.

Let's detail each operation with memory ordering:

- Loads of head and tail: acquire (to see latest updates).
- Loads of next pointers: acquire (to synchronize with stores that set them).
- CAS operations: we need acq_rel for read-modify-write (both acquire and release). For weak CAS loops we can use compare_exchange_weak with acq_rel for success, and relaxed for failure (commonly). We'll follow typical pattern: compare_exchange_weak(expected, desired, std::memory_order_acq_rel, std::memory_order_relaxed).

- Stores that other threads will observe: e.g., when we link new node via tail->next CAS, we want release so that the store of the node is visible after the CAS succeeds. So use release on success.

- When we advance tail, we want release.

- When we advance head after marking, we want release.

Now we need to handle the case where we mark hNext but then fail to move head; we leave it marked. Other threads will see h->next as marked when they load and get_unmarked_ref will give them the raw node (still marked?). Actually they will load h->next (atomic) which may be marked; they will call get_unmarked_ref on it to get underlying node. So they will still see the node as next. They may try to mark it again (CAS from expected to marked). If it's already marked, the CAS will fail because expected (unmarked) won't match. They could then try to help advance head by reading the marked node's next and moving head. So we need a helping step: after we see that h->next is marked, we can help advance head.

Thus in dequeue loop, after we load hNext = get_unmarked_ref(h->next.load...), we should also check if the original pointer (before unmarking) is marked. If it is marked, we should help advance head: compute succ = get_unmarked_ref( (marked_ptr)->next.load... ) and CAS head from h to succ. Then continue.

Simplify: In the loop, after loading h and t, we load rawNext = h->next.load(acquire). Then we compute next = get_unmarked_ref(rawNext). Also we compute bool marked = is_marked_ref(rawNext). If marked, we help advance head: Node* succ = get_unmarked_ref(next->next.load(acquire)); head.compare_exchange_weak(h, succ, acq_rel, relaxed); continue.

If not marked, then proceed as before: if h == t (empty or tail lagging) etc.

Now for the case where we have a valid next (non-null) and not marked, we attempt to mark it.

Implementation steps:

while (true) {
    Node* h = head.load(std::memory_order_acquire);
    Node* t = tail.load(std::memory_order_acquire);
    Node* rawNext = h->next.load(std::memory_order_acquire);
    Node* next = get_unmarked_ref(rawNext);
    bool marked = is_marked_ref(rawNext);

    if (marked) {
        // help advance head over marked node
        Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
        head.compare_exchange_weak(h, succ,
                                   std::memory_order_acq_rel,
                                   std::memory_order_relaxed);
        continue;
    }

    // re-check head to avoid ABA? We'll reload head after checking for changes?
    if (h != head.load(std::memory_order_acquire)) {
        continue; // head changed, retry
    }

    if (h == t) {
        // empty or tail lagging
        if (next == nullptr) {
            return -1;
        }
        // tail lagging
        tail.compare_exchange_weak(t, next,
                                   std::memory_order_acq_rel,
                                   std::memory_order_relaxed);
        continue;
    }

    // h != t and next != nullptr (since if next==nullptr, queue would be empty but h!=t cannot happen? Actually could happen if tail lagging and list has nodes but tail not updated yet; then next != nullptr. So fine.)
    // Attempt to logically mark next
    Node* markedNext = get_marked_ref(next);
    if (h->next.compare_exchange_weak(rawNext, markedNext,
                                      std::memory_order_acq_rel,
                                      std::memory_order_relaxed)) {
        // marking succeeded
        // Now try to advance head to skip the marked node
        Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
        if (head.compare_exchange_weak(h, succ,
                                       std::memory_order_acq_rel,
                                       std::memory_order_relaxed)) {
            // Successfully moved head; we can delete the marked node
            int v = next->val;
            delete next;
            return v;
        }
        // if head CAS failed, someone else will help; continue loop
        continue;
    }
    // if marking CAS failed, retry
}

Note: In the marking CAS, we compare rawNext (the current value of h->next) with markedNext. If it fails, maybe another thread marked it or advanced head, so we loop.

Now we need to ensure we don't dereference a null pointer when accessing next->next. In the case where next is nullptr (empty), we already handled earlier (return -1). In the marking block, we know next != nullptr because we checked h==t case and if next==nullptr we returned -1. Also we have h != t, so there is at least one node, so next != nullptr.

Now after marking, we access next->next.load. That's safe.

Now we need to define Node class inside the ConcurrentDataStructure or outside. We'll define it as a private struct inside the class.

Now we need to include <atomic>, <climits>, <cstdint>, "../utils/QueueADT.hpp".

Now we need to define constructor: allocate dummy node with value INT_MIN (or any). We'll set dummy->next = nullptr.

Now destructor: we need to traverse and delete all nodes. We'll start from head.load(std::memory_order_acquire). Then while (node != nullptr) { Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire)); delete node; node = next; }.

But we need to be careful: the dummy node may be marked? It shouldn't be. We'll just delete all nodes reachable via next pointers (unmarked). Since we may have marked nodes in the list, get_unmarked_ref will strip marks, so we will still traverse through them (the underlying node). However we may delete a node that is still marked and maybe still referenced by other threads? But destructor assumes no concurrent access.

Now we need to ensure we include <utility>? Not needed.

Now we need to ensure we mark the node's next pointer for logical deletion as per requirement: we marked the node itself (by setting head->next to marked reference). The requirement said "Mark the node's next pointer using get_marked_ref()". That's ambiguous but we marked the pointer to the node (head->next). We'll assume it's acceptable.

Now we need to ensure we never use a marked node as predecessor for CAS: we only use h (head) as predecessor for marking, and we never use markedNext as predecessor for CAS (we only read its next). Good.

Now we need to ensure we skip marked nodes during traversal: we use get_unmarked_ref when loading next pointers.

Now we need to ensure linearizability: The linearization point of enqueue is the successful CAS linking the new node to tail->next. The linearization point of dequeue is the successful CAS that marks the node (logical deletion) or maybe the CAS that moves head? Usually linearization point is the successful CAS that updates head. But we have marking step first; the logical deletion occurs when we mark the node; the physical unlink (head move) may happen later by same or another thread. However the value is returned after we successfully move head and before we delete node. The linearization point could be the successful head CAS (since that's when the node is removed from the queue). We'll assume that's fine.

Now we need to implement isEmpty() as described.

Now we need to ensure we include the header "../utils/QueueADT.hpp". We'll just include it.

Now we need to write the code in a single .hpp file.

Now we need to ensure we don't use any locks.

Now we need to ensure we use correct includes: #pragma once, #include "../utils/QueueADT.hpp", #include <atomic>, #include <climits>, #include <cstdint>.

Now we need to define class ConcurrentDataStructure : public QueueADT.

Now we need to define Node struct inside class.

Now we need to define static helper functions.

Now we need to implement constructor, destructor, enqueue, dequeue, isEmpty.

Now we need to ensure we use std::atomic<Node*> for head and tail.

Now we need to ensure we use proper memory ordering.

Now we need to test for correctness mentally.

Let's walk through enqueue:

- Initially head and tail point to dummy D (next=nullptr).
- Enqueue val1: allocate node N1 (val1, next=nullptr).
   Loop:
      t = tail.load() = D
      tNext = get_unmarked_ref(t->next.load()) = nullptr
      expected = nullptr
      CAS t->next from nullptr to N1 succeeds (release).
      Then CAS tail from D to N1 (release). May succeed or fail; if fails, loop again.
   After success, tail points to N1.

- Enqueue val2: allocate N2.
   Loop:
      t = tail.load() maybe N1 (if tail updated) or D (if lagging).
      If t = N1:
          tNext = get_unmarked_ref(N1->next.load()) = nullptr (since N1->next is nullptr)
          CAS N1->next from nullptr to N2 succeeds.
          CAS tail from N1 to N2 (maybe succeeds).
      If t = D (lagging):
          tNext = get_unmarked_ref(D->next.load()) = N1 (since D->next points to N1)
          Since tNext != nullptr, we CAS tail from D to N1 (try to advance tail).
          Then loop again.

Works.

Now dequeue:

Assume list: D -> N1 -> N2 -> ... ; tail points to last node.

- h = head.load() = D
- t = tail.load() = last
- rawNext = D->next.load() = N1 (unmarked)
- next = N1
- marked = false
- h != t (unless only one node)
- h == t? false
- Attempt to mark: CAS D->next from N1 to marked N1.
   If succeeds:
      succ = get_unmarked_ref(N1->next.load()) = N2
      CAS head from D to N2.
      If succeeds: return N1->val, delete N1.
   If head CAS fails (maybe another thread moved head already), we continue loop.

If marking CAS fails (maybe another thread already marked N1 or moved head), we loop.

Now consider case where only dummy node (empty):
- h = D, t = D
- rawNext = D->next.load() = nullptr
- next = nullptr
- marked = false
- h == t true
- if next == nullptr => return -1.

Now consider case where tail lagging: after some enqueues, tail may be behind. In dequeue, we check h == t; if true and next != nullptr, we try to advance tail to next (CAS). This helps tail catch up.

Now consider case where we marked a node but failed to move head. Example: after marking N1, another thread also marks N1 (CAS fails) or moves head. The node remains marked. In next iteration, we load h = head (maybe still D if head not moved). rawNext = D->next.load() will be marked N1 (since it's marked). We get next = get_unmarked_ref(rawNext) = N1, marked = true. Then we go into marked block: help advance head: succ = get_unmarked_ref(N1->next.load()) = N2; CAS head from D to N2. If succeeds, we continue loop (no value returned). Then next iteration will see head = N2, etc. The marked node N