#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
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
    
    std::atomic<Node*> head;
    
public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX, nullptr);
        Node* sentinel = new Node(INT_MIN, tail);
        head.store(sentinel, std::memory_order_relaxed);
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* next = curr->next.load(std::memory_order_relaxed);
            next = get_unmarked_ref(next);
            delete curr;
            curr = next;
        }
    }
    
    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        while (curr && curr->val < key) {
            Node* next = curr->next.load(std::memory_order_acquire);
            next = get_unmarked_ref(next);
            curr = next;
        }
        return curr && curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                } else {
                    if (curr->val >= key) {
                        if (curr->val == key) {
                            return false;
                        }
                        Node* new_node = new Node(key, curr);
                        if (pred->next.compare_exchange_strong(curr, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            return true;
                        }
                        delete new_node;
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                } else {
                    if (curr->val != key) {
                        if (curr->val > key) {
                            return false;
                        }
                        pred = curr;
                        curr = succ;
                    } else {
                        Node* marked_succ = get_marked_ref(succ);
                        if (!curr->next.compare_exchange_strong(succ, marked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            continue;
                        }
                        Node* unmarked_curr = get_unmarked_ref(curr);
                        Node* unmarked_succ = get_unmarked_ref(succ);
                        pred->next.compare_exchange_strong(unmarked_curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        return true;
                    }
                }
            }
        }
    }
};