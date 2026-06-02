#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public StackADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;

        explicit Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> top;

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
    ConcurrentDataStructure() : top(nullptr) {}

    ~ConcurrentDataStructure() override {
        Node* curr = get_unmarked_ref(top.load(std::memory_order_acquire));

        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    void push(int val) override {
        Node* new_node = new Node(val);

        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            while (curr_top && is_marked_ref(curr_top)) {
                Node* unmarked = get_unmarked_ref(curr_top);
                Node* next = get_unmarked_ref(
                    unmarked->next.load(std::memory_order_acquire)
                );

                top.compare_exchange_weak(
                    curr_top,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                curr_top = top.load(std::memory_order_acquire);
            }

            new_node->next.store(
                curr_top,
                std::memory_order_relaxed
            );

            if (top.compare_exchange_weak(
                    curr_top,
                    new_node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return;
            }
        }
    }

    int pop() override {
        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            if (!curr_top) {
                return INT_MIN;
            }

            if (is_marked_ref(curr_top)) {
                Node* unmarked = get_unmarked_ref(curr_top);
                Node* next = get_unmarked_ref(
                    unmarked->next.load(std::memory_order_acquire)
                );

                top.compare_exchange_weak(
                    curr_top,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                continue;
            }

            Node* next = curr_top->next.load(std::memory_order_acquire);

            if (is_marked_ref(next)) {
                top.compare_exchange_weak(
                    curr_top,
                    get_unmarked_ref(next),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                continue;
            }

            Node* marked_next = get_marked_ref(next);

            if (!curr_top->next.compare_exchange_weak(
                    next,
                    marked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            Node* expected = curr_top;

            top.compare_exchange_strong(
                expected,
                get_unmarked_ref(next),
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );

            int result = curr_top->val;
            delete curr_top;
            return result;
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            if (!curr_top) {
                return true;
            }

            if (!is_marked_ref(curr_top)) {
                Node* next = curr_top->next.load(std::memory_order_acquire);

                if (!is_marked_ref(next)) {
                    return false;
                }
            }

            Node* unmarked = get_unmarked_ref(curr_top);
            Node* next = get_unmarked_ref(
                unmarked->next.load(std::memory_order_acquire)
            );

            top.compare_exchange_weak(
                curr_top,
                next,
                std::memory_order_acq_rel,
                std::memory_order_acquire
            );
        }
    }
};
