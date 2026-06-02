#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/*
 * Versioned Linked List - Lock-free list with versioned try-locks
 * 
 * Based on "A Concurrency-Optimal List-Based Set"
 * by Gramoli, Kuznetsov, Ravi, Shang (DISC'15)
 * 
 * Key features:
 * - Wait-free contains operation
 * - Versioned try-lock for optimal concurrency
 * - Logical deletion with physical removal
 * - Version-based validation to avoid unnecessary locking
 */

class VersionedList : public SetADT {
private:
    // Versioned lock type: LSB is lock bit, remaining bits are version
    using verlock_t = uint32_t;
    
    struct Node {
        int val;
        Node* next;
        bool deleted;
        std::atomic<verlock_t> lock;
        
        Node(int v, Node* n = nullptr) 
            : val(v), next(n), deleted(false), lock(0) {}
    };
    
    Node* head;
    
    // Get the version from a versioned lock (mask out lock bit)
    static verlock_t get_version(const std::atomic<verlock_t>& lock) {
        return lock.load(std::memory_order_acquire) & ~1U;
    }
    
    // Try to lock at a specific version
    // Returns true if successful (version matched and lock acquired)
    static bool try_lock_at_version(std::atomic<verlock_t>& lock, verlock_t version) {
        verlock_t expected = version;
        return lock.compare_exchange_strong(
            expected, 
            version + 1,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }
    
    // Spin until lock is acquired
    static void spinlock(std::atomic<verlock_t>& lock) {
        while (!try_lock_at_version(lock, get_version(lock))) {
            // Spin
        }
    }
    
    // Unlock and increment version
    static void unlock_and_increment_version(std::atomic<verlock_t>& lock) {
        lock.fetch_add(1, std::memory_order_release);
    }
    
    // Wait-free traverse to find position for val
    // Records prev node along the way
    void traverse(int val, Node** prev, Node** curr, Node* start) {
        *prev = start;
        *curr = start;
        
        while ((*curr)->val < val) {
            *prev = *curr;
            *curr = (*curr)->next;
        }
    }
    
    // Short traversal that validates and records version of prev
    // Returns false if full abort needed (prev deleted)
    bool validate(int val, Node** prev, Node** curr, verlock_t* prev_version) {
        while (true) {
            *prev_version = get_version((*prev)->lock);
            
            if ((*prev)->deleted) {
                // Full abort needed
                return false;
            }
            
            *curr = (*prev)->next;
            while ((*curr)->val < val) {
                *prev_version = get_version((*curr)->lock);
                
                if ((*curr)->deleted) {
                    // Partial abort - restart validation from current prev
                    continue;
                }
                
                *prev = *curr;
                *curr = (*curr)->next;
            }
            
            // Successfully validated
            return true;
        }
    }

public:
    VersionedList() {
        // Create sentinel nodes
        Node* tail = new Node(INT_MAX, nullptr);
        head = new Node(INT_MIN, tail);
    }
    
    ~VersionedList() override {
        Node* curr = head;
        while (curr) {
            Node* next = curr->next;
            delete curr;
            curr = next;
        }
    }
    
    // Wait-free contains operation
    bool contains(int key) override {
        Node* curr = head;
        
        while (curr->val < key) {
            curr = curr->next;
        }
        
        // Value is present and not logically deleted
        return (curr->val == key && !curr->deleted);
    }
    
    // Insert with versioned locking
    bool add(int key) override {
        Node* prev = nullptr;
        Node* curr = nullptr;
        Node* newnode = nullptr;
        verlock_t prev_version;
        
        // Full abort: restart from traversal
        while (true) {
            traverse(key, &prev, &curr, head);
            
            // Partial abort: restart from validate
            while (true) {
                // Pre-locking validation
                if (!validate(key, &prev, &curr, &prev_version)) {
                    // prev is no longer appropriate, traverse again
                    break;  // Go to outer loop
                }
                
                // Value is logically deleted
                if (curr->deleted) {
                    break;  // Go to outer loop
                }
                
                // Value already exists
                if (curr->val == key) {
                    if (newnode) delete newnode;
                    return false;
                }
                
                // Pre-allocate new node if not already done
                if (newnode == nullptr) {
                    newnode = new Node(key, nullptr);
                }
                
                // Pre-link new node to next node
                newnode->next = curr;
                
                // Attempt to lock at validated version
                if (!try_lock_at_version(prev->lock, prev_version)) {
                    // Version changed, need to validate again
                    continue;  // Stay in inner loop
                }
                
                // Link new node to prev
                prev->next = newnode;
                
                unlock_and_increment_version(prev->lock);
                
                return true;
            }
        }
    }
    
    // Remove with versioned locking
    bool remove(int key) override {
        Node* prev = nullptr;
        Node* curr = nullptr;
        verlock_t prev_version;
        
        // Full abort: restart from traversal
        while (true) {
            traverse(key, &prev, &curr, head);
            
            // Partial abort: restart from validate
            while (true) {
                // Pre-locking validation
                if (!validate(key, &prev, &curr, &prev_version)) {
                    // prev is no longer appropriate, traverse again
                    break;  // Go to outer loop
                }
                
                // Value not present or already deleted
                if (curr->val != key || curr->deleted) {
                    return false;
                }
                
                // Attempt to lock at validated version
                if (!try_lock_at_version(prev->lock, prev_version)) {
                    // Version changed, need to validate again
                    continue;  // Stay in inner loop
                }
                
                // Lock current node
                spinlock(curr->lock);
                
                // Logical delete
                curr->deleted = true;
                
                // Physical delete
                prev->next = curr->next;
                
                unlock_and_increment_version(curr->lock);
                unlock_and_increment_version(prev->lock);
                
                return true;
            }
        }
    }
};
