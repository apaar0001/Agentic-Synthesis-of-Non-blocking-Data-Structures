#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 1024;
    
    struct Node {
        int val;
        std::atomic<Node*> next;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };
    
    std::atomic<Node*> buckets[BUCKET_COUNT];
    
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }
    
    std::size_t hash(int key) const {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }
    
    bool find(int key, Node*& pred, Node*& curr, std::size_t idx) {
        Node* pred_next;
        
        while (true) {
            pred = buckets[idx].load(std::memory_order_acquire);
            curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == nullptr) {
                    return false;
                }
                
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(curr)) {
                    pred_next = get_unmarked_ref(curr);
                    if (!pred->next.compare_exchange_strong(curr, pred_next, std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = pred_next;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    return unmarked_curr->val == key;
                }
                
                pred = unmarked_curr;
                curr = succ;
            }
        }
    }
    
public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_release);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr != nullptr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred;
        Node* curr;
        
        return find(key, pred, curr, idx);
    }
    
    bool add(int key) override {
        std::size_t idx = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            
            if (find(key, pred, curr, idx)) {
                return false;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* new_node = new Node(key, unmarked_curr);
            
            Node* expected = unmarked_curr;
            if (pred->next.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        std::size_t idx = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            
            if (!find(key, pred, curr, idx)) {
                return false;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!unmarked_curr->next.compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                continue;
            }
            
            Node* expected = unmarked_curr;
            if (pred->next.compare_exchange_strong(expected, succ, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                delete unmarked_curr;
            }
            
            return true;
        }
    }
};