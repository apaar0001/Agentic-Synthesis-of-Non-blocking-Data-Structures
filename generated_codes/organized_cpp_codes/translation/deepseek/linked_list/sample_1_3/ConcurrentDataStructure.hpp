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

    bool search(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head.load(std::memory_order_acquire);
            curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                if (curr == tail.load(std::memory_order_acquire)) {
                    return false;
                }
                
                Node* succ = curr->next.load(std::memory_order_acquire);
                
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_weak(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = unmarked_succ;
                    if (curr == tail.load(std::memory_order_acquire)) {
                        return false;
                    }
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (curr->val >= key) {
                    return curr != tail.load(std::memory_order_acquire) && curr->val == key;
                }
                
                pred = curr;
                curr = succ;
            }
            
        retry:
            continue;
        }
    }

public:
    ConcurrentDataStructure() {
        Node* t = new Node(INT_MAX);
        t->next.store(nullptr, std::memory_order_relaxed);
        Node* h = new Node(INT_MIN, t);
        head.store(h, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
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
        while (curr != tail.load(std::memory_order_acquire)) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                curr = get_unmarked_ref(succ);
                continue;
            }
            if (curr->val == key) {
                return true;
            }
            if (curr->val > key) {
                return false;
            }
            curr = succ;
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            bool found = search(key, pred, curr);
            
            if (found) {
                return false;
            }
            
            Node* newNode = new Node(key, curr);
            if (pred->next.compare_exchange_weak(curr, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            bool found = search(key, pred, curr);
            
            if (!found) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!curr->next.compare_exchange_weak(succ, marked_succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            // Node has been marked
            pred->next.compare_exchange_weak(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};