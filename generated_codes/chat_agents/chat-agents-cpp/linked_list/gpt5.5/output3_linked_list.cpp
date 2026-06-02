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

        explicit Node(int v) : val(v), next(nullptr) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool find(int key, Node** pred_out, Node** curr_out) {
    retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));

        while (true) {
            Node* succ;

            while (true) {
                Node* next = curr->next.load(std::memory_order_acquire);
                if (!is_marked_ref(next)) {
                    succ = get_unmarked_ref(next);
                    break;
                }

                succ = get_unmarked_ref(next);
                Node* expected = curr;

                if (!pred->next.compare_exchange_strong(
                        expected,
                        succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    goto retry;
                }

                curr = succ;
            }

            if (curr->val >= key) {
                *pred_out = pred;
                *curr_out = curr;
                return true;
            }

            pred = curr;
            curr = succ;
        }
    }

public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX);
        head = new Node(INT_MIN);
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;

        while (curr) {
            Node* next = curr->next.load(std::memory_order_relaxed);
            next = get_unmarked_ref(next);
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head;

        while (curr->val < key) {
            curr = get_unmarked_ref(
                curr->next.load(std::memory_order_acquire));
        }

        Node* next = curr->next.load(std::memory_order_acquire);

        return curr->val == key && !is_marked_ref(next);
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;

            find(key, &pred, &curr);

            if (curr->val == key) {
                return false;
            }

            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);

            Node* expected = curr;

            if (pred->next.compare_exchange_strong(
                    expected,
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

            find(key, &pred, &curr);

            if (curr->val != key) {
                return false;
            }

            Node* succ = curr->next.load(std::memory_order_acquire);

            if (!is_marked_ref(succ)) {
                Node* marked = get_marked_ref(get_unmarked_ref(succ));

                if (curr->next.compare_exchange_strong(
                        succ,
                        marked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {

                    Node* expected = curr;

                    pred->next.compare_exchange_strong(
                        expected,
                        get_unmarked_ref(succ),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire);

                    return true;
                }
            }
        }
    }
};
