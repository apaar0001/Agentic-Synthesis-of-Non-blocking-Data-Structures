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

    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }

public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX);
        tail->next.store(nullptr, std::memory_order_relaxed);
        head.store(new Node(INT_MIN, tail), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* temp = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete temp;
        }
    }

    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        
        while (curr->val < key) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                curr = get_unmarked_ref(succ);
            } else {
                curr = succ;
            }
        }
        
        return curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                    continue;
                }
                
                if (curr->val >= key) {
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            if (curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            
            if (pred->next.compare_exchange_strong(curr, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked_succ;
                    continue;
                }
                
                if (curr->val >= key) {
                    break;
                }
                
                pred = curr;
                curr = succ;
            }
            
            if (curr->val != key) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_succ,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            // Node has been marked
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};