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

        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);

        while (true) {
            Node* old_top = top.load(std::memory_order_acquire);

            while (old_top != nullptr &&
                   is_marked_ref(old_top->next.load(std::memory_order_acquire))) {
                Node* next = get_unmarked_ref(
                    old_top->next.load(std::memory_order_acquire));

                top.compare_exchange_weak(
                    old_top,
                    next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);

                old_top = top.load(std::memory_order_acquire);
            }

            node->next.store(old_top, std::memory_order_relaxed);

            if (top.compare_exchange_weak(
                    old_top,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return;
            }
        }
    }

    int pop() override {
        while (true) {
            Node* old_top = top.load(std::memory_order_acquire);

            if (old_top == nullptr) {
                return INT_MIN;
            }

            Node* next = old_top->next.load(std::memory_order_acquire);

            if (is_marked_ref(next)) {
                Node* unmarked_next = get_unmarked_ref(next);

                top.compare_exchange_weak(
                    old_top,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire);

                continue;
            }

            Node* marked_next = get_marked_ref(next);

            if (!old_top->next.compare_exchange_strong(
                    next,
                    marked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            Node* expected = old_top;

            if (top.compare_exchange_strong(
                    expected,
                    get_unmarked_ref(next),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                int result = old_top->val;
                delete old_top;
                return result;
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* curr = top.load(std::memory_order_acquire);

            if (curr == nullptr) {
                return true;
            }

            Node* next = curr->next.load(std::memory_order_acquire);

            if (!is_marked_ref(next)) {
                return false;
            }

            Node* unmarked_next = get_unmarked_ref(next);

            top.compare_exchange_weak(
                curr,
                unmarked_next,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
        }
    }
};
