#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <random>
#include <vector>
#include <cstdint>
#include <cassert>

/*
 * NoHotspot Skip List - Lock-free skip list with reduced head contention
 * 
 * Based on Fraser's skip list but with padding and optimizations to reduce
 * contention at the head node, which is a common hotspot in skip lists.
 * 
 * Key features:
 * - Lock-free operations using CAS
 * - Marked pointers for logical deletion
 * - Cache line padding to reduce false sharing
 * - Optimized head node access patterns
 */

class NoHotspotSkipList : public SetADT {
private:
    static constexpr int NUM_LEVELS = 20;
    static constexpr unsigned long SENTINEL_KEYMIN = 1UL;
    static constexpr unsigned long SENTINEL_KEYMAX = ~0UL;
    static constexpr int CACHE_LINE_SIZE = 64;
    
    struct Node {
        unsigned long key;
        int level;
        // Padding to reduce false sharing
        char pad1[CACHE_LINE_SIZE - sizeof(unsigned long) - sizeof(int)];
        std::atomic<Node*> next[NUM_LEVELS];
        char pad2[CACHE_LINE_SIZE];
        
        Node(unsigned long k, int lev) : key(k), level(lev) {
            for (int i = 0; i < NUM_LEVELS; i++) {
                next[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };
    
    Node* head;
    std::mt19937 rng;
    std::mutex rngMutex;
    
    // Marked pointer helpers
    static bool is_marked(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1UL) != 0;
    }
    
    static Node* get_unmarked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1UL);
    }
    
    static Node* get_marked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1UL);
    }
    
    // Convert caller key to internal key
    static unsigned long caller_to_internal(int k) {
        return static_cast<unsigned long>(k) + 2;
    }
    
    // Random level generator
    int get_level() {
        std::lock_guard<std::mutex> lock(rngMutex);
        unsigned long r = rng();
        int l = 1;
        r = (r >> 4) & ((1 << (NUM_LEVELS-1)) - 1);
        while ((r & 1) && l < NUM_LEVELS) {
            l++;
            r >>= 1;
        }
        return l;
    }
    
    // Strong search with physical deletion
    Node* strong_search_predecessors(unsigned long k, Node** preds, Node** succs) {
    retry:
        std::atomic_thread_fence(std::memory_order_acquire);
        
        Node* x = head;
        for (int i = NUM_LEVELS - 1; i >= 0; i--) {
            Node* x_next = x->next[i].load(std::memory_order_acquire);
            
            if (is_marked(x_next)) goto retry;
            
            Node* y = x_next;
            while (true) {
                Node* y_next = y->next[i].load(std::memory_order_acquire);
                while (is_marked(y_next)) {
                    y = get_unmarked(y_next);
                    y_next = y->next[i].load(std::memory_order_acquire);
                }
                
                unsigned long y_k = y->key;
                if (y_k >= k) break;
                
                x = y;
                x_next = y_next;
            }
            
            if (x_next != y) {
                Node* expected = x_next;
                if (!x->next[i].compare_exchange_strong(expected, y,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
            }
            
            if (preds) preds[i] = x;
            if (succs) succs[i] = y;
        }
        
        return succs ? succs[0] : nullptr;
    }
    
    // Weak search - optimistic
    Node* weak_search_predecessors(unsigned long k, Node** preds, Node** succs) {
        Node* x = head;
        for (int i = NUM_LEVELS - 1; i >= 0; i--) {
            Node* x_next;
            while (true) {
                x_next = x->next[i].load(std::memory_order_acquire);
                x_next = get_unmarked(x_next);
                
                unsigned long x_next_k = x_next->key;
                if (x_next_k >= k) break;
                
                x = x_next;
            }
            
            if (preds) preds[i] = x;
            if (succs) succs[i] = x_next;
        }
        
        return succs ? succs[0] : nullptr;
    }
    
    // Mark node deleted at all levels
    void mark_deleted(Node* x, int level) {
        for (int i = level - 1; i >= 0; i--) {
            Node* x_next = x->next[i].load(std::memory_order_acquire);
            while (!is_marked(x_next)) {
                Node* expected = x_next;
                x->next[i].compare_exchange_strong(expected, get_marked(x_next),
                    std::memory_order_acq_rel, std::memory_order_acquire);
                x_next = x->next[i].load(std::memory_order_acquire);
            }
            std::atomic_thread_fence(std::memory_order_release);
        }
    }

public:
    NoHotspotSkipList() : rng(std::random_device{}()) {
        head = new Node(SENTINEL_KEYMIN, NUM_LEVELS);
        Node* tail = new Node(SENTINEL_KEYMAX, NUM_LEVELS);
        
        for (int i = 0; i < NUM_LEVELS; i++) {
            head->next[i].store(tail, std::memory_order_relaxed);
        }
    }
    
    ~NoHotspotSkipList() override {
        Node* curr = head;
        while (curr && curr->key != SENTINEL_KEYMAX) {
            Node* next = get_unmarked(curr->next[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
        if (curr) delete curr;
    }
    
    bool contains(int key) override {
        unsigned long k = caller_to_internal(key);
        Node* x = weak_search_predecessors(k, nullptr, nullptr);
        return (x && x->key == k);
    }
    
    bool add(int key) override {
        unsigned long k = caller_to_internal(key);
        
        Node* preds[NUM_LEVELS];
        Node* succs[NUM_LEVELS];
        Node* newNode = nullptr;
        
        Node* succ = weak_search_predecessors(k, preds, succs);
        
    retry:
        if (succ && succ->key == k) {
            if (newNode) delete newNode;
            return false;
        }
        
        if (!newNode) {
            int level = get_level();
            newNode = new Node(k, level);
            
            for (int i = 0; i < level; i++) {
                newNode->next[i].store(succs[i], std::memory_order_relaxed);
            }
        }
        
        int level = newNode->level;
        
        std::atomic_thread_fence(std::memory_order_release);
        Node* expected = succ;
        if (!preds[0]->next[0].compare_exchange_strong(expected, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            succ = strong_search_predecessors(k, preds, succs);
            goto retry;
        }
        
        for (int i = 1; i < level; i++) {
            while (true) {
                Node* pred = preds[i];
                succ = succs[i];
                
                Node* new_next = newNode->next[i].load(std::memory_order_acquire);
                if (is_marked(new_next)) {
                    return true;
                }
                
                if (new_next != succ) {
                    expected = new_next;
                    if (!newNode->next[i].compare_exchange_strong(expected, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (is_marked(expected)) {
                            return true;
                        }
                    }
                }
                
                if (succ->key == k) {
                    std::atomic_thread_fence(std::memory_order_acquire);
                    strong_search_predecessors(k, preds, succs);
                    continue;
                }
                
                expected = succ;
                if (pred->next[i].compare_exchange_strong(expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    break;
                }
                
                std::atomic_thread_fence(std::memory_order_acquire);
                strong_search_predecessors(k, preds, succs);
            }
        }
        
        return true;
    }
    
    bool remove(int key) override {
        unsigned long k = caller_to_internal(key);
        
        Node* preds[NUM_LEVELS];
        Node* x = weak_search_predecessors(k, preds, nullptr);
        
        if (!x || x->key != k) {
            return false;
        }
        
        int level = x->level;
        mark_deleted(x, level);
        
        for (int i = level - 1; i >= 0; i--) {
            Node* expected = x;
            if (preds[i]->next[i].compare_exchange_strong(expected, 
                    get_unmarked(x->next[i].load(std::memory_order_acquire)),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            } else {
                std::atomic_thread_fence(std::memory_order_seq_cst);
                strong_search_predecessors(k, nullptr, nullptr);
                break;
            }
        }
        
        return true;
    }
};
