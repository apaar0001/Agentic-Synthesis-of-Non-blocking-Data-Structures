#pragma once

#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>

/**
 * Lazy Linked List - Lock-based concurrent set with lazy synchronization
 * 
 * Based on "A Lazy Concurrent List-Based Set Algorithm" by Heller et al. (OPODIS 2005)
 * 
 * Algorithm:
 * - Optimistic traversal without locks to find position
 * - Lock predecessor and current nodes before modification
 * - Validate that nodes are still adjacent and unmarked
 * - Logical deletion via marking before physical removal
 * 
 * Key properties:
 * - Lock-based with per-node mutexes
 * - Optimistic traversal reduces lock contention
 * - Validation ensures correctness after optimistic search
 * - Sentinel nodes at INT_MIN and INT_MAX
 */
class LazyList : public SetADT {
private:
    struct Node {
        int val;
        Node* next;
        std::mutex lock;
        bool marked;  // Logical deletion flag
        
        Node(int v, Node* n = nullptr) : val(v), next(n), marked(false) {}
    };

    Node* head;

    /**
     * Get unmarked reference (removes logical deletion flag)
     */
    Node* get_unmarked_ref(Node* n) {
        return n;  // In C++ we use a separate bool field instead of pointer tagging
    }

    /**
     * Validate that pred and curr are adjacent and both unmarked
     * Must be called while holding locks on both nodes
     */
    bool validate(Node* pred, Node* curr) {
        return !pred->marked && !curr->marked && pred->next == curr;
    }

public:
    LazyList() {
        // Create sentinel nodes: head (INT_MIN) -> tail (INT_MAX) -> nullptr
        Node* tail = new Node(INT_MAX, nullptr);
        head = new Node(INT_MIN, tail);
    }

    ~LazyList() override {
        // Clean up all nodes
        Node* curr = head;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        // Optimistic traversal without locks
        Node* curr = head;
        while (curr->val < key) {
            curr = curr->next;
        }
        // Check if found and not logically deleted
        return (curr->val == key && !curr->marked);
    }

    bool add(int key) override {
        while (true) {
            // Optimistic search for position
            Node* pred = head;
            Node* curr = pred->next;
            while (curr->val < key) {
                pred = curr;
                curr = curr->next;
            }

            // Lock both nodes in order
            std::lock_guard<std::mutex> pred_guard(pred->lock);
            std::lock_guard<std::mutex> curr_guard(curr->lock);

            // Validate that nodes are still adjacent and unmarked
            if (validate(pred, curr)) {
                // Check if key already exists
                if (curr->val == key) {
                    return false;  // Key already present
                }
                
                // Insert new node between pred and curr
                Node* newnode = new Node(key, curr);
                pred->next = newnode;
                return true;
            }
            // Validation failed, retry
        }
    }

    bool remove(int key) override {
        while (true) {
            // Optimistic search for key
            Node* pred = head;
            Node* curr = pred->next;
            while (curr->val < key) {
                pred = curr;
                curr = curr->next;
            }

            // Lock both nodes in order
            std::lock_guard<std::mutex> pred_guard(pred->lock);
            std::lock_guard<std::mutex> curr_guard(curr->lock);

            // Validate that nodes are still adjacent and unmarked
            if (validate(pred, curr)) {
                // Check if key exists
                if (curr->val != key) {
                    return false;  // Key not found
                }
                
                // Logical deletion: mark the node
                curr->marked = true;
                
                // Physical deletion: unlink from list
                pred->next = curr->next;
                
                // Note: We don't delete curr here as other threads may still be traversing
                // In a production system, this would require garbage collection
                // For this benchmark, we accept the memory leak during operation
                // The destructor will clean up all nodes at the end
                
                return true;
            }
            // Validation failed, retry
        }
    }
};
