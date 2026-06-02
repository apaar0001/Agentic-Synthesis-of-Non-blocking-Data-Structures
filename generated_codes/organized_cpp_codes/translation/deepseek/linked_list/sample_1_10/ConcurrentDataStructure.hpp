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

    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    void cleanup(Node* start) {
        Node* curr = start;
        while (curr) {
            Node* temp = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete temp;
        }
    }

public:
    ConcurrentDataStructure() {
        Node* t = new Node(INT_MAX);
        t->next.store(nullptr, std::memory_order_relaxed);
        
        Node* h = new Node(INT_MIN);
        h->next.store(t, std::memory_order_relaxed);
        
        head.store(h, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        cleanup(head.load(std::memory_order_relaxed));
    }

    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        while (true) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                return false;
            }
            
            if (is_marked_ref(curr)) {
                curr = unmarked_curr->next.load(std::memory_order_acquire);
                continue;
            }
            
            if (unmarked_curr->val == key) {
                return true;
            }
            if (unmarked_curr->val > key) {
                return false;
            }
            
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
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
                
                Node* next = unmarked_curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(curr)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_next,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        continue;
                    }
                    curr = unmarked_next;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    break;
                }
                
                pred = unmarked_curr;
                curr = next;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr != tail.load(std::memory_order_acquire) && unmarked_curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            
            if (pred->next.compare_exchange_strong(curr, newNode,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                return true;
            }
            
            delete newNode;
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
                
                Node* next = unmarked_curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(curr)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_next,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        continue;
                    }
                    curr = unmarked_next;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    break;
                }
                
                pred = unmarked_curr;
                curr = next;
            }
            
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == tail.load(std::memory_order_acquire) || unmarked_curr->val != key) {
                return false;
            }
            
            Node* next = unmarked_curr->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(next);
            
            if (unmarked_curr->next.compare_exchange_strong(next, marked_next,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                // Node has been marked
                Node* expected = curr;
                Node* unmarked_next = get_unmarked_ref(next);
                pred->next.compare_exchange_strong(expected, unmarked_next,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
                return true;
            }
        }
    }
};