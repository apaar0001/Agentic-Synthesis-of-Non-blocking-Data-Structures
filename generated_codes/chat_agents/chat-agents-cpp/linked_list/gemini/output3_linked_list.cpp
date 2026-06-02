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

        Node(int key) : val(key), next(nullptr) {}
    };

    Node* head;
    Node* tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool find(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head;
            curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break; 
                    }
                    curr = unmarked_succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }

                if (is_marked_ref(succ)) {
                    break; 
                }

                if (curr->val >= key) {
                    return curr->val == key;
                }
                
                pred = curr;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            if (find(key, pred, curr)) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            if (!find(key, pred, curr)) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (!is_marked_ref(succ)) {
                if (curr->next.compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    if (pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // Physically unlinked
                    }
                    return true;
                }
            }
        }
    }

    bool contains(int key) override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->val >= key) {
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                return unmarked_curr->val == key && !is_marked_ref(succ);
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        return false;
    }
};
