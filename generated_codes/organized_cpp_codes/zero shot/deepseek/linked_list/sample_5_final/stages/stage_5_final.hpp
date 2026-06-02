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
        Node* tail = new Node(INT_MAX);
        Node* sentinel = new Node(INT_MIN, tail);
        head.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* unmarked = get_unmarked_ref(curr);
            Node* next = unmarked->next.load(std::memory_order_relaxed);
            next = get_unmarked_ref(next);
            delete unmarked;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked = get_unmarked_ref(curr);
            if (unmarked->val == key) {
                return !is_marked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (unmarked->val > key) {
                return false;
            }
            curr = unmarked->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);

            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);

                if (is_marked_ref(succ)) {
                    Node* marked = get_marked_ref(unmarked_curr);
                    pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    break;
                }

                if (unmarked_curr->val >= key) {
                    if (unmarked_curr->val == key) {
                        return false;
                    }

                    Node* new_node = new Node(key, unmarked_curr);
                    if (pred->next.compare_exchange_strong(curr, new_node,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }

                    delete new_node;
                    break;
                }

                pred = unmarked_curr;
                curr = succ;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);

            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);

                if (is_marked_ref(succ)) {
                    Node* marked = get_marked_ref(unmarked_curr);
                    pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    break;
                }

                if (unmarked_curr->val > key) {
                    return false;
                }

                if (unmarked_curr->val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (!unmarked_curr->next.compare_exchange_strong(succ, marked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        continue;
                    }

                    pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    return true;
                }

                pred = unmarked_curr;
                curr = succ;
            }
        }
    }
};