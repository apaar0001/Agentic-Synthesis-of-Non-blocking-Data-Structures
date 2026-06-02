#pragma once

#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>

/**
 * LockCouplingList - Hand-over-hand locking linked list
 * 
 * This implementation uses hand-over-hand (lock coupling) locking where
 * threads acquire locks on two consecutive nodes at a time, then release
 * the previous lock while keeping the current lock and acquiring the next.
 * This allows for fine-grained concurrency while maintaining correctness.
 * 
 * Algorithm:
 * 1. Lock head and head->next
 * 2. While traversing: unlock previous, keep current locked, lock next
 * 3. This ensures at least one lock is always held during traversal
 * 
 * Sentinel nodes at INT_MIN and INT_MAX ensure no special cases for
 * empty list or boundary conditions.
 */
class LockCouplingList : public SetADT {
private:
    struct Node {
        int val;
        Node* next;
        std::mutex lock;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    Node* head;

public:
    LockCouplingList() {
        // Create sentinel nodes: head (INT_MIN) -> tail (INT_MAX) -> nullptr
        Node* tail = new Node(INT_MAX, nullptr);
        head = new Node(INT_MIN, tail);
    }

    ~LockCouplingList() override {
        // Clean up all nodes
        Node* curr = head;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        // Hand-over-hand locking: lock two nodes at a time
        std::unique_lock<std::mutex> curr_lock(head->lock);
        Node* curr = head;
        
        std::unique_lock<std::mutex> next_lock(curr->next->lock);
        Node* next = curr->next;
        
        // Traverse until we find a node >= key
        while (next->val < key) {
            curr_lock.unlock();  // Release previous lock
            curr = next;
            curr_lock = std::move(next_lock);  // Current becomes previous
            
            next_lock = std::unique_lock<std::mutex>(next->next->lock);
            next = next->next;
        }
        
        bool found = (next->val == key);
        // Locks automatically released by RAII
        return found;
    }

    bool add(int key) override {
        // Hand-over-hand locking
        std::unique_lock<std::mutex> curr_lock(head->lock);
        Node* curr = head;
        
        std::unique_lock<std::mutex> next_lock(curr->next->lock);
        Node* next = curr->next;
        
        // Find insertion point
        while (next->val < key) {
            curr_lock.unlock();
            curr = next;
            curr_lock = std::move(next_lock);
            
            next_lock = std::unique_lock<std::mutex>(next->next->lock);
            next = next->next;
        }
        
        // Check if key already exists
        bool found = (next->val == key);
        if (!found) {
            // Insert new node between curr and next
            Node* newnode = new Node(key, next);
            curr->next = newnode;
        }
        
        // Locks automatically released
        return !found;
    }

    bool remove(int key) override {
        // Hand-over-hand locking
        std::unique_lock<std::mutex> curr_lock(head->lock);
        Node* curr = head;
        
        std::unique_lock<std::mutex> next_lock(curr->next->lock);
        Node* next = curr->next;
        
        // Find node to remove
        while (next->val < key) {
            curr_lock.unlock();
            curr = next;
            curr_lock = std::move(next_lock);
            
            next_lock = std::unique_lock<std::mutex>(next->next->lock);
            next = next->next;
        }
        
        bool found = (next->val == key);
        if (found) {
            // Remove next node by unlinking it
            curr->next = next->next;
            next_lock.unlock();  // Release lock before deletion
            delete next;
        }
        
        // Locks automatically released
        return found;
    }
};
