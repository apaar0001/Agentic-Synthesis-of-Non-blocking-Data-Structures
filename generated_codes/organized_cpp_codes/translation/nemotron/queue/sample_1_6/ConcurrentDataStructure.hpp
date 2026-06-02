        class SetADT {
        public:
            virtual ~SetADT() = default;
            virtual bool contains(int key) = 0;
            virtual bool add(int key) = 0;
            virtual bool remove(int key) = 0;
        };

   And we are to include "../utils/SetADT.hpp", but the user said to include "../utils/QueueADT.hpp" in the concurrent version? 

   However, the latest instruction does not specify the include for SetADT.

   But note: the user said in the concurrent version instructions:
        "Include: #pragma once, <atomic>, <climits>, <cstdint>, "../utils/QueueADT.hpp"."

   But that was for the queue version. Now we are doing a set.

   Since the problem has changed, we will include "../utils/SetADT.hpp" if it exists, but the user might have meant QueueADT by mistake.

   However, the latest instruction says to inherit from SetADT.

   To be safe, we will include "../utils/SetADT.hpp", but note the user's initial instruction for the concurrent version said to include "../utils/QueueADT.hpp".

   Given the confusion, and since the user now says to inherit from SetADT, we will assume that the SetADT header is available at "../utils/SetADT.hpp".

   If not, we might have to change it to QueueADT, but the methods don't match.

   Let's look at the error we got earlier: 
        error: no viable conversion from returned value of type 'unique_ptr<ConcurrentDataStructure>' to function return type 'unique_ptr<SetADT>'

   This indicates that the factory is expected to return a SetADT.

   Therefore, we will include "../utils/SetADT.hpp".

   However, the user's initial instruction for the concurrent version said to include "../utils/QueueADT.hpp", but that was when they thought it was a queue.

   Now we are doing a set, so we change the include.

   But note: the user said in the concurrent version instructions: 
        "Include: #pragma once, <atomic>, <climits>, <cstdint>, "../utils/QueueADT.hpp"."

   We are going against that by including SetADT.

   However, the latest instruction overrides: inherit from SetADT.

   We'll do:

        #pragma once
        #include "../utils/SetADT.hpp"
        #include <atomic>
        #include <climits>
        #include <cstdint>

   If the SetADT is not available, we might get an error, but we follow the latest instruction.

   Now, let's code the set.

   Steps:

        - Dummy head (INT_MIN) and dummy tail (INT_MAX).
        - Head and tail are atomic pointers.

   Note: The head and tail pointers themselves are not changed after initialization (except that we might help in bypassing marked nodes, but the dummy nodes are never removed).

   We'll make head and tail atomic<Node*>.

   Constructor:

        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);

   Destructor: we traverse and delete all nodes (including the dummies). But note: we must be cautious because other threads might be using the nodes.
        However, the destructor is called when no more threads are using the data structure (assuming proper lifetime management).

   We'll do:

        Node* cur = head.load(std::memory_order_acquire);
        while (cur) {
            Node* next = cur->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);
            delete cur;
            cur = unmarked_next;
        }

   But note: the tail node's next is null, so we will delete the tail and then cur becomes null.

   Now, the operations:

        bool contains(int key) override {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                // Skip marked nodes
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    // Help to remove the marked node by bypassing it
                    if (pred->next.compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        succ = unmarked_succ->next.load(std::memory_order_acquire);
                    } else {
                        break;
                    }
                }
                if (curr->key >= key) {
                    break;
                }
                pred = curr;
                curr = get_unmarked_ref(succ);
            }
            return (curr != tail.load(std::memory_order_acquire)) && (curr->key == key) && (!is_marked_ref(curr));
        }

        However, note: we check that curr is not marked? Actually, in the traversal we skipped marked nodes in the chain, but curr itself might be marked?
        How? Because we set curr from get_unmarked_ref(succ) and we also set pred from curr (which was unmarked at the time we read it).

        But note: after we read curr, it might be marked by another thread before we use it.

        Therefore, we should check if curr is marked at the point of use.

        We can do:

            if (curr->key >= key) {
                // Check if curr is marked
                if (is_marked_ref(curr)) {
                    // Then we treat it as not present? But note: we skipped marked nodes in the chain, but curr might have been marked after we read it.
                    // We should restart? Or we can check and if marked, then we continue to skip?
                    // Actually, we can treat a marked curr as if it were not there, so we should continue the search from pred.
                    // But note: we have pred->next pointing to a marked curr, so we should help to remove it and then retry.
                    // However, to keep it simple, we can check and if curr is marked, then we do the same as if we found a marked node in the chain.
                    // But note: we are at the point where we have pred and curr, and we are about to check curr->key.
                    // If curr is marked, then we should help to remove it and then retry the search from pred.
                    // However, we are not in the inner while loop that skips marked nodes in the chain.
                    // So we can do:
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* marked_curr = curr; // which is marked
                    if (pred->next.compare_exchange_strong(marked_curr, unmarked_curr->next.load(std::memory_order_acquire),
                            std::memory_order_release, std::memory_order_relaxed)) {
                        // Successfully helped to remove the marked curr
                        // Now, we need to restart the search from pred? Or we can set curr to unmarked_curr->next and continue?
                        // But note: we are in the outer loop, so we can continue without updating pred and curr? 
                        // Actually, we break out of the inner while and then we are here. We can set curr to the next of the removed node and continue the outer loop without moving pred.
                        // However, to avoid complexity, we can simply restart the entire contains operation? 
                        // But that might be inefficient.
                        // Alternatively, we can set curr = unmarked_curr->next.load() and then continue the outer loop (without updating pred) because we know that pred->next now points to unmarked_curr->next.
                        // But note: we have already helped to remove the marked curr, so the list from pred is now pred -> (unmarked_curr->next) -> ...
                        // So we set curr = unmarked_curr->next.load() and then continue the outer loop (without changing pred).
                        curr = unmarked_curr->next.load(std::memory_order_acquire);
                        continue; // to the top of the while(true) loop
                    } else {
                        // Failed to help, so we restart the search from the beginning? 
                        // Or we can try again from pred? 
                        // We'll break and let the outer loop handle it by re-reading.
                        // But note: we are in the middle of the outer loop iteration.
                        // We'll break out of the if and then the outer loop will continue? 
                        // Actually, we break out of the if and then we are after the if, so we will check curr->key again? 
                        // But we haven't updated curr.
                        // This is messy.
                    }
                }
                // ... rest
            }
        }

   To avoid this complexity, we can note that in the traversal we always ensure that the nodes we follow (via get_unmarked_ref) are unmarked at the time we read the pointer, but they might become marked later.

   However, the standard Harris-Michael set does not check the current node for being marked in the outer loop because:

        - We set curr from get_unmarked_ref(succ) where succ was read from curr->next, and we then skipped marked nodes in the chain (so the succ we used to get curr was unmarked at the time of read, but note: we did get_unmarked_ref on a marked succ to get an unmarked node, but that unmarked node might be marked later?).

   Actually, the standard approach is:

        Node* pred = head;
        Node* curr = pred->next;
        while (curr->key < key) {
            pred = curr;
            curr = curr->next;
            // But we must skip marked nodes in the chain? 
            // Instead, we do:
            while (is_marked_ref(curr)) {
                // help to remove curr
                Node* succ = curr->next;
                Node* unmarked_succ = get_unmarked_ref(succ);
                if (pred->next.compare_exchange_strong(curr, unmarked_succ, ...)) {
                    curr = unmarked_succ;
                } else {
                    break;
                }
            }
        }

   But note: the above might not be wait-free.

   We'll use the common traversal that skips marked nodes at every step.

   Revised traversal for contains (and similarly for add and remove):

        Node* pred = head.load(std::memory_order_acquire);
        Node* curr = pred->next.load(std::memory_order_acquire);
        while (true) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            // Skip any marked nodes after curr
            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                // Help to remove the marked node by making pred->next point to unmarked_succ
                if (pred->next.compare_exchange_weak(curr, unmarked_succ,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    succ = unmarked_succ->next.load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
            if (curr->key >= key) {
                break;
            }
            pred = curr;
            curr = get_unmarked_ref(succ);
        }

   Then, for contains, we check if curr->key == key and curr is not the tail and not marked? 
   But note: we have not checked if curr is marked. However, we just came from get_unmarked_ref(succ) which returns an unmarked node? 
   Not exactly: get_unmarked_ref(succ) returns the unmarked version of a pointer that might be marked, but the node itself might be marked? 
   No: the marking is on the pointer, not on the node. The node is never marked; it's the pointer to the node that is marked.

   Therefore, the node pointed to by an unmarked pointer is not logically deleted.

   However, note: we set curr = get_unmarked_ref(succ). The variable succ was read from curr->next, and we then skipped marked nodes in the chain (so we know that the succ we used to compute curr was either unmarked or we helped to bypass marked nodes until we got an unmarked succ). 
   But note: the node that succ points to (after unmarking) is not marked (because the mark is on the pointer, and we removed it by get_unmarked_ref).

   However, the node itself might have been logically deleted by having its incoming pointer marked? 
   But note: a node is logically deleted if the pointer that points to it (from its predecessor) is marked.

   In our traversal, we have:
        pred -> [pointer to curr] 
   and we have not checked if that pointer is marked.

   How do we know that the pointer from pred to curr is not marked? 
   We don't. We only know that we got curr by: 
        curr = get_unmarked_ref(succ_prev)   [where succ_prev was the next of the previous pred]

   But note: in the previous step, we had:
        pred_prev -> [pointer to curr_prev] 
        and we read succ_prev = curr_prev->next
        then we skipped marked nodes in the chain starting from succ_prev to get an unmarked node, say X.
        then we set pred = curr_prev and curr = X.

   And we know that the pointer from pred_prev to curr_prev was not marked at the time we read it? 
   Not necessarily: we did not check.

   Therefore, we must check at the point of use whether the node curr is logically deleted, which is indicated by the pointer from pred to curr being marked.

   But note: we have pred and curr, and we can check the pointer from pred to curr by looking at pred->next.

   However, we just read pred->next to get curr? Not exactly: we set curr from get_unmarked_ref(succ) where succ was from the previous curr's next.

   Actually, in the loop, we have:
        We read: 
            curr = ... (from the previous iteration's get_unmarked_ref(succ))
        then we read succ = curr->next
        then we skip marked nodes in the chain after curr.

   We do not check the pointer from pred to curr.

   So after we break out of the inner while (that skips marked nodes after curr), we have:
        pred -> [pointer to curr]   (and we have not checked if that pointer is marked)
        and curr -> [pointer to succ]   (and we have made sure that succ is unmarked by skipping marked nodes in the chain after curr)

   But note: the pointer from pred to curr might be marked.

   How can that happen? 
        Suppose: 
            pred -> [marked A] -> B -> ...
        and we are at pred and curr = A (but A is marked). 
        How did we get curr = A?
        In the previous iteration, we had:
            pred_prev -> ... -> pred
            and we read succ = pred->next, which was [marked A] -> ...
            then we skipped marked nodes: 
                succ = [marked A] -> we get unmarked_succ = A (by get_unmarked_ref) and then we look at A->next = B.
                then we tried to CAS pred->next from [marked A] to B? 
                But note: we did not do that in the inner while for skipping marked nodes after curr? 
                Actually, in the inner while, we are skipping marked nodes after the current curr (which was pred in that iteration).

        Let me trace:

            Iteration 1:
                pred = head
                curr = head->next = [marked A]   (but note: we read it as a marked pointer)
                then we read succ = curr->next = A->next = B
                then we enter the inner while: 
                    succ is [marked A]? -> no, succ is B (unmarked) because we read A->next as B (and A->next is not marked unless someone marked it, but we assume it's not marked for now).
                so we break the inner while.
                then we check: curr->key = A->key. 
                If A->key < key, then we set pred = curr (which is [marked A]) and curr = get_unmarked_ref(succ) = B.

            Now, in the next iteration:
                pred = [marked A]   (but note: pred is a pointer that we stored, and it is marked? 
                However, we stored the pointer value which was marked. But when we use pred, we do:
                    curr = pred->next.load()
                But pred is a marked pointer? -> This is a problem because we cannot dereference a marked pointer.

        Therefore, we must never store a marked pointer in pred or curr.

   How to avoid? 
        We must ensure that when we set pred and curr, they are unmarked pointers.

   Standard solution: 
        In the traversal, after we read a pointer, we immediately get its unmarked version and use that for the node pointer, but we keep the original (possibly marked) pointer for the purpose of CAS.

   However, for the purpose of following the list, we only want to follow unmarked pointers.

   Revised traversal (as in Harris-Michael):

        Node* pred = head;
        Node* curr = nullptr;
        while (true) {
            curr = pred->next.load(std::memory_order_acquire);
            // Get unmarked version of curr for following, but keep the original for CAS if needed
            Node* curr_unmarked = get_unmarked_ref(curr);
            if (curr_unmarked == nullptr) break; // reached end

            Node* succ = curr_unmarked->next.load(std::memory_order_acquire);
            // Skip marked nodes after curr_unmarked
            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                // Help to remove the marked node by making pred->next point to unmarked_succ
                if (pred->next.compare_exchange_weak(curr, unmarked_succ,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    succ = unmarked_succ->next.load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
            if (curr_unmarked->key >= key) {
                break;
            }
            pred = curr_unmarked;
        }

   But note: we are storing pred as an unmarked pointer.

   However, when we do pred->next.load(), we are loading from an unmarked pointer, which is safe.

   And we use the original curr (which might be marked) in the CAS.

   Let's adjust:

        We'll let:
            pred: always an unmarked pointer (to a node that is not logically deleted)
            curr: the raw pointer read from pred->next (which might be marked)

        Steps:

            pred = head (unmarked, because head is dummy and never marked)
            while (true) {
                curr = pred->next.load(std::memory_order_acquire);   // might be marked
                if (curr == nullptr) break;   // but note: we have a tail, so curr won't be null until after tail? 
                // Actually, our tail is the last node and its next is null, so when pred is tail, curr = null.

                Node* curr_unmarked = get_unmarked_ref(curr);
                if (curr_unmarked == nullptr) { 
                    // This should not happen because if curr is null, then get_unmarked_ref(null)=null, and we break.
                    // But if curr is a marked null? -> null marked is still null.
                    break;
                }

                Node* succ = curr_unmarked->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    // Help to remove the marked node (which is curr_unmarked->next) by bypassing it
                    if (pred->next.compare_exchange_weak(curr, unmarked_succ,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        succ = unmarked_succ->next.load(std::memory_order_acquire);
                    } else {
                        break;
                    }
                }
                if (curr_unmarked->key >= key) {
                    break;
                }
                pred = curr_unmarked;
            }

   Then, for contains:
        We have pred (unmarked) and curr (raw pointer from pred->next).
        We then check:
            if (curr == get_unmarked_ref(tail)) -> then we are at the tail, so not found.
            else if (curr_unmarked->key == key) -> then found, but we must also check that the node is not logically deleted? 
                However, note: we have not checked if the pointer from pred to curr is marked. 
                But if it is marked, then curr_unmarked->next might be something else, but we have not removed it.

        Actually, the node curr_unmarked is logically deleted if and only if the pointer from pred to curr is marked.

        How to check that? 
            We can check: is_marked_ref(pred->next.load()) 
            but note: we just loaded pred->next into curr, and then we computed curr_unmarked from it.
            However, between the load and the check, it might have been changed.

        So we should check at the point of use:

            if (is_marked_ref(curr)) {
                // then the node curr_unmarked is logically deleted
                // so we treat it as not present
                return false;
            } else {
                return (curr_unmarked->key == key);
            }

        But note: we have already skipped marked nodes in the chain after curr_unmarked, but not the pointer to curr_unmarked.

   Alternatively, we can note that if the pointer from pred to curr is marked, then we should have helped to remove it in the inner while of the previous iteration? 
   Not necessarily, because we are at the first node.

   Given the complexity, and since the problem says we can leak, we will do the following in contains:

        After the traversal, we have:
            pred: unmarked pointer to a node
            curr: raw pointer from pred->next (might be marked)
            curr_unmarked = get_unmarked_ref(curr)

        Then, we check if curr_unmarked is the tail -> return false.

        Then, we check if the pointer from pred to curr is marked: 
            if (is_marked_ref(curr)) {
                // then the node is logically deleted -> not present
                return false;
            }
        else {
            return (curr_unmarked->key == key);
        }

   However, note: it is possible that curr is marked and curr_unmarked->key == key, but we return false because it's deleted.

   This is correct.

   But note: we must also ensure that curr_unmarked is not the tail? 
        We break the loop when curr_unmarked->key >= key, and the tail has key=INT_MAX, so if key <= INT_MAX, we might break at the tail.

        We should check: if (curr_unmarked == tail) then return false.

   However, we have a tail node with key=INT_MAX, and we break when curr_unmarked->key >= key.
        If key is greater than INT_MAX, that's impossible, so we assume key is in [INT_MIN, INT_MAX].

   So if curr_unmarked->key == INT_MAX, then we are at the tail.

   We can do:

        if (curr_unmarked == tail) {
            return false;
        }
        if (is_marked_ref(curr)) {
            return false;
        }
        return (curr_unmarked->key == key);

   But note: we have the tail node stored, so we can compare pointers.

   However, we must be cautious: the tail node might be replaced? No, we never remove the tail.

   Now, for add and remove, we use a similar traversal.

   Let's implement add:

        bool add(int key) override {
            while (true) {
                Node* pred = head.load(std::memory_order_acquire);
                Node* curr = pred->next.load(std::memory_order_acquire);
                while (true) {
                    Node* curr_unmarked = get_unmarked_ref(curr);
                    Node* succ = curr_unmarked->next.load(std::memory_order_acquire);
                    while (is_marked_ref(succ)) {
                        Node* unmarked_succ = get_unmarked_ref(succ);
                        // Help to remove the marked node (succ) by making curr_unmarked->next point to unmarked_succ
                        if (curr_unmarked->next.compare_exchange_weak(succ, unmarked_succ,
                                std::memory_order_release, std::memory_order_relaxed)) {
                            succ = unmarked_succ->next.load(std::memory_order_acquire);
                        } else {
                            break;
                        }
                    }
                    if (curr_unmarked->key >= key) {
                        break;
                    }
                    pred = curr_unmarked;
                    curr = curr_unmarked->next.load(std::memory_order_acquire);
                }
                Node* curr_unmarked = get_unmarked_ref(curr);
                // Check if we found the key
                if (curr_unmarked != get_unmarked_ref(tail.load(std::memory_order_acquire)) && 
                    curr_unmarked->key == key) {
                    // But note: we must check if the node is not marked? 
                    // However, if the node is marked, then we should treat it as not present? 
                    // But wait: if the node is marked, then it is logically deleted, so we can add it again? 
                    // However, the set should not have duplicates, and a marked node is considered removed.
                    // So if we find a node with the key that is marked, we can try to add by unmarking it? 
                    // But the problem does not specify. 
                    // Standard behavior: if the element is present (even if marked for deletion) then add should fail? 
                    // Or should it succeed by unmarking? 
                    // The Harris-Michael set does not allow adding an element that is marked for deletion; it considers it present.
                    // But note: the marked node is still in the list (logically deleted) so the set still contains it? 
                    // No: logically deleted means it is removed from the set.
                    // Therefore, if we find a node with the key that is marked, then the key is not in the set.
                    // However, we have not physically removed it, so it is still in the list but marked.
                    // The set does not contain it.
                    // So we should be able to add it.
                    // But note: we cannot have two nodes with the same key.
                    // So if there is a marked node with the key, we cannot add another node with the same key because it would break the sorted order? 
                    // Actually, we could replace the marked node with a new unmarked node? 
                    // But the standard is to not allow duplicates, and a marked node is considered as not present for the purpose of the set, but we cannot have two nodes with the same key.
                    // Therefore, we must treat a marked node with the key as if the key is not present, but we cannot insert a duplicate key.
                    // So we should fail the add if there is any node (marked or unmarked) with the key? 
                    // No: because the marked node is considered removed, so the key is not in the set, but we cannot insert a duplicate key because the list must not have duplicate keys.
                    // Therefore, we must fail the add if there is a node (marked or unmarked) with the key in the list at the correct sorted position.
                    // However, note: the list is sorted and we are inserting in sorted order, so if there is a node with the key (marked or unmarked) at the position where we would insert, then we fail.
                    // But wait: if there is a marked node with the key, then the key is not in the set, but we still cannot insert another node with the same key because it would break the uniqueness.
                    // So we should fail.
                    //
                    // Therefore, we check: if we found a node (curr_unmarked) with curr_unmarked->key == key, then we fail, regardless of whether it is marked or not.
                    // However, note: we have not checked if the node is marked for the purpose of the key comparison? 
                    // The key is stored in the node and is not affected by marking.
                    //
                    // So we do:
                    //   if (curr_unmarked->key == key) then return false.
                    //
                    // But note: we broke the loop when curr_unmarked->key >= key.
                    // So if curr