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
        
        Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX);
        Node* sentinel = new Node(INT_MIN);
        sentinel->next.store(tail, std::memory_order_relaxed);
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
        while (curr && get_unmarked_ref(curr)->val < key) {
            curr = get_unmarked_ref(curr)->next.load(std::memory_order_acquire);
        }
        return curr && get_unmarked_ref(curr)->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                                                           std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (!curr || get_unmarked_ref(curr)->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (curr && get_unmarked_ref(curr)->val == key) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            
            if (pred->next.compare_exchange_strong(curr, new_node,
                                                  std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                                                           std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (!curr || get_unmarked_ref(curr)->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (!curr || get_unmarked_ref(curr)->val != key) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!curr->next.compare_exchange_strong(succ, marked_succ,
                                                   std::memory_order_acq_rel)) {
                continue;
            }
            
            pred->next.compare_exchange_strong(curr, succ,
                                              std::memory_order_acq_rel);
            return true;
        }
    }
};