#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 1024;
    
    struct Node {
        int val;
        std::atomic<Node*> next;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };
    
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }
    
    std::atomic<Node*> buckets[BUCKET_COUNT];
    
    size_t hash(int key) const {
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }
    
    void cleanup(Node* node) {
        while (node) {
            Node* next = get_unmarked_ref(node->next.load(std::memory_order_relaxed));
            delete node;
            node = next;
        }
    }
    
    bool find(int key, Node*& pred, Node*& curr, size_t bucket_idx) {
        while (true) {
            Node* pred_ptr = nullptr;
            Node* curr_ptr = nullptr;
            Node* next_ptr = nullptr;
            
            pred_ptr = buckets[bucket_idx].load(std::memory_order_acquire);
            curr_ptr = pred_ptr;
            
            while (true) {
                if (!curr_ptr) {
                    pred = pred_ptr;
                    curr = curr_ptr;
                    return false;
                }
                
                next_ptr = curr_ptr->next.load(std::memory_order_acquire);
                Node* unmarked_next = get_unmarked_ref(next_ptr);
                
                if (is_marked_ref(next_ptr)) {
                    Node* unmarked_curr = get_unmarked_ref(curr_ptr);
                    if (!pred_ptr->next.compare_exchange_strong(curr_ptr, unmarked_next, std::memory_order_acq_rel)) {
                        break;
                    }
                    delete unmarked_curr;
                    curr_ptr = unmarked_next;
                    continue;
                }
                
                if (curr_ptr->val >= key) {
                    pred = pred_ptr;
                    curr = curr_ptr;
                    return curr_ptr->val == key;
                }
                
                pred_ptr = curr_ptr;
                curr_ptr = unmarked_next;
            }
        }
    }
    
public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_release);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = buckets[i].load(std::memory_order_relaxed);
            cleanup(head);
        }
    }
    
    bool contains(int key) override {
        size_t bucket_idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        
        bool found = find(key, pred, curr, bucket_idx);
        if (!found) return false;
        
        Node* next = curr->next.load(std::memory_order_acquire);
        return !is_marked_ref(next);
    }
    
    bool add(int key) override {
        size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            
            if (find(key, pred, curr, bucket_idx)) {
                Node* next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                return false;
            }
            
            Node* new_node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            
            if (!find(key, pred, curr, bucket_idx)) {
                return false;
            }
            
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                return false;
            }
            
            Node* marked_next = get_marked_ref(next);
            if (!curr->next.compare_exchange_strong(next, marked_next, std::memory_order_acq_rel)) {
                continue;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (pred->next.compare_exchange_strong(curr, next, std::memory_order_acq_rel)) {
                delete unmarked_curr;
            }
            
            return true;
        }
    }
};