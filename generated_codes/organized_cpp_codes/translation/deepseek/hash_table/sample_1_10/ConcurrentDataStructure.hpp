#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    
    struct Node {
        int key;
        std::atomic<Node*> next;
        
        explicit Node(int k) : key(k), next(nullptr) {}
    };
    
    std::atomic<Node*> buckets[BUCKET_COUNT];
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    std::size_t hash(int key) const {
        return static_cast<std::size_t>(key) % BUCKET_COUNT;
    }
    
    void find_nodes(std::size_t bucket_idx, int key, Node*& pred, Node*& curr, Node*& succ) {
        retry:
        pred = buckets[bucket_idx].load(std::memory_order_acquire);
        curr = pred->next.load(std::memory_order_acquire);
        
        while (true) {
            if (!curr) {
                succ = nullptr;
                return;
            }
            
            succ = curr->next.load(std::memory_order_acquire);
            
            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                if (!pred->next.compare_exchange_weak(curr, unmarked_succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = unmarked_succ;
                if (!curr) {
                    succ = nullptr;
                    return;
                }
                succ = curr->next.load(std::memory_order_acquire);
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (!curr || unmarked_curr->key >= key) {
                return;
            }
            
            pred = curr;
            curr = succ;
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* sentinel = new Node(INT_MIN);
            sentinel->next.store(nullptr, std::memory_order_release);
            buckets[i].store(sentinel, std::memory_order_release);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete get_unmarked_ref(curr);
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        std::size_t bucket_idx = hash(key);
        Node* curr = buckets[bucket_idx].load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        
        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->key == key && !is_marked_ref(curr)) {
                return true;
            }
            if (unmarked_curr->key > key) {
                return false;
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        return false;
    }
    
    bool add(int key) override {
        std::size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            Node* succ;
            find_nodes(bucket_idx, key, pred, curr, succ);
            
            if (curr && get_unmarked_ref(curr)->key == key) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_release);
            
            if (pred->next.compare_exchange_weak(curr, new_node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        std::size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            Node* succ;
            find_nodes(bucket_idx, key, pred, curr, succ);
            
            if (!curr || get_unmarked_ref(curr)->key != key) {
                return false;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* expected_succ = succ;
            Node* marked_succ = get_marked_ref(expected_succ);
            
            if (!unmarked_curr->next.compare_exchange_weak(expected_succ, marked_succ,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            
            Node* expected_curr = curr;
            Node* unmarked_succ = get_unmarked_ref(succ);
            pred->next.compare_exchange_weak(expected_curr, unmarked_succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            
            return true;
        }
    }
};