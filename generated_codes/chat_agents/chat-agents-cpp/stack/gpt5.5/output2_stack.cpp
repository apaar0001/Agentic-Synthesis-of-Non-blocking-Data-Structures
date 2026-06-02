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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    void help_remove_marked_top() {
        while (true) {
            Node* curr = top.load(std::memory_order_acquire);

            if (curr == nullptr) {
                return;
            }

            Node* next = curr->next.load(std::memory_order_acquire);

            if (!is_marked_ref(next)) {
                return;
            }

            Node* unmarked_next = get_unmarked_ref(next);

            if (top.compare_exchange_weak(
                    curr,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                delete curr;
            }
        }
    }

public:
    ConcurrentDataStructure() : top(nullptr) {}

    ~ConcurrentDataStructure() override {
        Node* curr = get_unmarked_ref(top.load(std::memory_order_acquire));

        while (curr != nullptr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);
            delete curr;
            curr = unmarked_next;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);

        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            while (curr_top != nullptr) {
                Node* next = curr_top->next.load(std::memory_order_acquire);

                if (!is_marked_ref(next)) {
                    break;
                }

                help_remove_marked_top();
                curr_top = top.load(std::memory_order_acquire);
            }

            node->next.store(curr_top, std::memory_order_relaxed);

            if (top.compare_exchange_weak(
                    curr_top,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return;
            }
        }
    }

    int pop() override {
        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            if (curr_top == nullptr) {
                return INT_MIN;
            }

            Node* next = curr_top->next.load(std::memory_order_acquire);

            if (is_marked_ref(next)) {
                help_remove_marked_top();
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
            Node* unmarked_next = get_unmarked_ref(next);

            if (top.compare_exchange_strong(
                    expected,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                int value = curr_top->val;
                delete curr_top;
                return value;
            }

            help_remove_marked_top();
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* curr_top = top.load(std::memory_order_acquire);

            if (curr_top == nullptr) {
                return true;
            }

            Node* next = curr_top->next.load(std::memory_order_acquire);

            if (!is_marked_ref(next)) {
                return false;
            }

            help_remove_marked_top();
        }
    }
};
