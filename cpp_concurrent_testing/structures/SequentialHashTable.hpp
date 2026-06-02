#pragma once

#include "../utils/SetADT.hpp"
#include <climits>
#include <cstdlib>

/**
 * Sequential Hash Table - No concurrency control
 * 
 * Simple hash table with linked list buckets, no locks.
 * NOT thread-safe - for single-threaded use or as ground truth reference.
 * 
 * Algorithm:
 * - Fixed array of buckets, each is a simple linked list
 * - Hash function distributes keys across buckets
 * - Operations delegate to the appropriate bucket
 * - No synchronization primitives
 * 
 * Key properties:
 * - Sequential (not thread-safe)
 * - Simple linked list per bucket
 * - Sentinel nodes at INT_MIN and INT_MAX per bucket
 * - Hash function: key % NUM_BUCKETS
 */
class SequentialHashTable : public SetADT {
private:
    static constexpr int NUM_BUCKETS = 1024;

    struct Node {
        int val;
        Node* next;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    struct Bucket {
        Node* head;
        
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
     * Find a key in a specific bucket
     */
    bool bucket_contains(Bucket& bucket, int key) {
        Node* curr = bucket.head->next;  // Skip sentinel head
        while (curr->val < key) {
            curr = curr->next;
        }
        return (curr->val == key);
    }

    /**
     * Add a key to a specific bucket
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
     * Remove a key from a specific bucket
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
    SequentialHashTable() {
        // Buckets are initialized by their constructors
    }

    ~SequentialHashTable() override {
        // Buckets are cleaned up by their destructors
    }

    bool contains(int key) override {
        int bucket_idx = hash(key);
        return bucket_contains(buckets[bucket_idx], key);
    }

    bool add(int key) override {
        int bucket_idx = hash(key);
        return bucket_add(buckets[bucket_idx], key);
    }

    bool remove(int key) override {
        int bucket_idx = hash(key);
        return bucket_remove(buckets[bucket_idx], key);
    }
};
