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

    static bool is_marked_ref(Node* node) {
        return (reinterpret_cast<uintptr_t>(node) & 1) != 0;
    }

    static Node* get_unmarked_ref(Node* node) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(node) & ~uintptr_t(1));
    }

    static Node* get_marked_ref(Node* node) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(node) | 1);
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* t = new Node(INT_MAX);
        Node* h = new Node(INT_MIN, t);
        head.store(h, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* current = head.load(std::memory_order_relaxed);
        while (current) {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        
        while (curr != tail.load(std::memory_order_acquire)) {
            if (curr->val == key) {
                Node* next = curr->next.load(std::memory_order_acquire);
                return !is_marked_ref(next);
            }
            if (curr->val > key) {
                return false;
            }
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                    break;
                }
                
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* expected = curr;
                    Node* desired = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(expected, desired,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        break;
                    }
                    curr = desired;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    break;
                }
                
                pred = unmarked_curr;
                curr = succ;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr != tail.load(std::memory_order_acquire) && unmarked_curr->val == key) {
                Node* next = unmarked_curr->next.load(std::memory_order_acquire);
                if (!is_marked_ref(next)) {
                    return false;
                }
                continue;
            }
            
            Node* new_node = new Node(key, curr);
            Node* expected = curr;
            if (pred->next.compare_exchange_strong(expected, new_node,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                    return false;
                }
                
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* expected = curr;
                    Node* desired = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(expected, desired,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        break;
                    }
                    curr = desired;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    break;
                }
                
                pred = unmarked_curr;
                curr = succ;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == tail.load(std::memory_order_acquire) || unmarked_curr->val != key) {
                return false;
            }
            
            Node* next = unmarked_curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                return false;
            }
            
            Node* marked_next = get_marked_ref(next);
            if (!unmarked_curr->next.compare_exchange_strong(next, marked_next,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            Node* succ = get_unmarked_ref(marked_next);
            pred->next.compare_exchange_strong(curr, succ,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
            return true;
        }
    }
};