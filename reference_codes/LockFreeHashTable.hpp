#pragma once
#include "../utils/SetADT.hpp"
#include "LockFreeList.hpp"
#include <memory>
#include <cmath>

/*
 * Lock-Free Hash Table
 * 
 * Simple hash table that delegates operations to lock-free lists per bucket.
 * Each bucket is an independent lock-free linked list.
 * 
 * Key features:
 * - Lock-free operations (delegated to LockFreeList)
 * - Fixed number of buckets
 * - Simple modulo hash function
 */

class LockFreeHashTable : public SetADT {
private:
    static constexpr int NUM_BUCKETS = 1024;
    std::unique_ptr<SetADT> buckets[NUM_BUCKETS];
    
    // Simple hash function
    int hash(int key) const {
        // Use absolute value to handle negative keys
        int absKey = (key < 0) ? -key : key;
        return absKey % NUM_BUCKETS;
    }

public:
    LockFreeHashTable() {
        // Initialize each bucket with a lock-free list
        for (int i = 0; i < NUM_BUCKETS; i++) {
            buckets[i] = std::make_unique<LockFreeList>();
        }
    }
    
    ~LockFreeHashTable() override {
        // unique_ptr handles cleanup automatically
    }
    
    bool contains(int key) override {
        return buckets[hash(key)]->contains(key);
    }
    
    bool add(int key) override {
        return buckets[hash(key)]->add(key);
    }
    
    bool remove(int key) override {
        return buckets[hash(key)]->remove(key);
    }
};
