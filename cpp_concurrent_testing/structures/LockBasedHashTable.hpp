#pragma once

#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>
#include <cstdlib>

/**
 * Lock-Based Hash Table - Bucket-level locking with linked list buckets
 * 
 * Based on synchrobench's hashtable-lock implementation
 * 
 * Algorithm:
 * - Fixed array of buckets, each is a simple linked list
 * - Each bucket has its own lock for fine-grained concurrency
 * - Hash function distributes keys across buckets
 * - Operations delegate to the appropriate bucket
 * 
 * Key properties:
 * - Lock-based with per-bucket mutexes
 * - Simple linked list per bucket (no lazy deletion)
 * - Sentinel nodes at INT_MIN and INT_MAX per bucket
 * - Hash function: key % NUM_BUCKETS
 */
class LockBasedHashTable : public SetADT {
private:
    static constexpr int NUM_BUCKETS = 1024;

    struct Node {
        int val;
        Node* next;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    struct Bucket {
        Node* head;
        std::mutex lock;
        
        Bucket() {
            // Create sentinel nodes: head (INT_MIN) -> tail (INT_MAX) -> nullptr
            Node* tail = new Node(INT_MAX, nullptr);
            head = new Node(INT_MIN, tail);
        }

        ~Bucket() {
            // Clean up all nodes in this bucket
            Node* curr = head;
            while (curr) {
                Node* next = curr->next;
                delete curr;
                curr = next;
            }
        }
    };

    Bucket buckets[NUM_BUCKETS];

    /**
     * Hash function to map keys to bucket indices
     */
    int hash(int key) const {
        // Use absolute value to handle negative keys, then modulo
        return std::abs(key) % NUM_BUCKETS;
    }

    /**
     * Find a key in a specific bucket (must hold bucket lock)
     */
    bool bucket_contains(Bucket& bucket, int key) {
        Node* curr = bucket.head->next;  // Skip sentinel head
        while (curr->val < key) {
            curr = curr->next;
        }
        return (curr->val == key);
    }

    /**
     * Add a key to a specific bucket (must hold bucket lock)
     */
    bool bucket_add(Bucket& bucket, int key) {
        Node* pred = bucket.head;
        Node* curr = pred->next;
        
        // Find position
        while (curr->val < key) {
            pred = curr;
            curr = curr->next;
        }
        
        // Check if already exists
        if (curr->val == key) {
            return false;
        }
        
        // Insert new node
        Node* newnode = new Node(key, curr);
        pred->next = newnode;
        return true;
    }

    /**
     * Remove a key from a specific bucket (must hold bucket lock)
     */
    bool bucket_remove(Bucket& bucket, int key) {
        Node* pred = bucket.head;
        Node* curr = pred->next;
        
        // Find key
        while (curr->val < key) {
            pred = curr;
            curr = curr->next;
        }
        
        // Check if exists
        if (curr->val != key) {
            return false;
        }
        
        // Remove node
        pred->next = curr->next;
        delete curr;
        return true;
    }

public:
    LockBasedHashTable() {
        // Buckets are initialized by their constructors
    }

    ~LockBasedHashTable() override {
        // Buckets are cleaned up by their destructors
    }

    bool contains(int key) override {
        int bucket_idx = hash(key);
        std::lock_guard<std::mutex> guard(buckets[bucket_idx].lock);
        return bucket_contains(buckets[bucket_idx], key);
    }

    bool add(int key) override {
        int bucket_idx = hash(key);
        std::lock_guard<std::mutex> guard(buckets[bucket_idx].lock);
        return bucket_add(buckets[bucket_idx], key);
    }

    bool remove(int key) override {
        int bucket_idx = hash(key);
        std::lock_guard<std::mutex> guard(buckets[bucket_idx].lock);
        return bucket_remove(buckets[bucket_idx], key);
    }
};
