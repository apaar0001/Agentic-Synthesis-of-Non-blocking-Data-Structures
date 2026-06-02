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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool is_marked_node(Node* node) {
        if (!node) return false;
        Node* next_node = node->next.load(std::memory_order_relaxed);
        return is_marked_ref(next_node);
    }

public:
    ConcurrentDataStructure() {
        Node* max_node = new Node(INT_MAX);
        Node* min_node = new Node(INT_MIN, max_node);
        head.store(min_node, std::memory_order_relaxed);
        tail.store(max_node, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* unmarked_next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = unmarked_next;
        }
    }

    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        Node* curr_next;

        while (curr) {
            curr_next = curr->next.load(std::memory_order_acquire);
            
            Node* unmarked_next = get_unmarked_ref(curr_next);
            
            if (curr->val == key && !is_marked_ref(curr_next)) {
                return true;
            }
            
            if (curr->val > key) {
                return false;
            }
            
            curr = unmarked_next;
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
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

    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (curr->val > key) {
                    return false;
                }
                
                if (curr->val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (!curr->next.compare_exchange_strong(succ, marked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        continue;
                    }
                    
                    pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    return true;
                }
                
                pred = curr;
                curr = succ;
            }
        }
    }
};