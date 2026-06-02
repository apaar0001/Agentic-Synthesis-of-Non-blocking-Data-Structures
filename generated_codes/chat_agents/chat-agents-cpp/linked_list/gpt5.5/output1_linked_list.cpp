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

        Node(int v, Node* n = nullptr)
            : val(v), next(n) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    bool find(int key, Node** pred_out, Node** curr_out) {
    retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(
            pred->next.load(std::memory_order_acquire));

        while (true) {
            Node* succ = curr->next.load(std::memory_order_acquire);

            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);

                if (!pred->next.compare_exchange_strong(
                        curr,
                        unmarked_succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    goto retry;
                }

                curr = unmarked_succ;
                succ = curr->next.load(std::memory_order_acquire);
            }

            if (curr->val >= key) {
                *pred_out = pred;
                *curr_out = curr;
                return (curr->val == key);
            }

            pred = curr;
            curr = get_unmarked_ref(succ);
        }
    }

public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX);
        head = new Node(INT_MIN, tail);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;

        while (curr != nullptr) {
            Node* next = get_unmarked_ref(
                curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head;

        while (curr->val < key) {
            Node* next = curr->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(next);
        }

        Node* succ = curr->next.load(std::memory_order_acquire);

        return (curr->val == key) && !is_marked_ref(succ);
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;

            if (find(key, &pred, &curr)) {
                return false;
            }

            Node* node = new Node(key, curr);

            if (pred->next.compare_exchange_strong(
                    curr,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;

            if (!find(key, &pred, &curr)) {
                return false;
            }

            Node* succ = curr->next.load(std::memory_order_acquire);

            if (is_marked_ref(succ)) {
                continue;
            }

            Node* marked_succ = get_marked_ref(succ);

            if (!curr->next.compare_exchange_strong(
                    succ,
                    marked_succ,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            pred->next.compare_exchange_strong(
                curr,
                succ,
                std::memory_order_acq_rel,
                std::memory_order_acquire);

            return true;
        }
    }
};
