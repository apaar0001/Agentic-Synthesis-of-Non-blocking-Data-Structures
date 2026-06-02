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

        Node(int v) : val(v), next(nullptr) {}
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
        Node* curr = top.load(std::memory_order_relaxed);
        while (curr != nullptr) {
            Node* next_node = get_unmarked_ref(curr)->next.load(std::memory_order_relaxed);
            delete get_unmarked_ref(curr);
            curr = next_node;
        }
    }

    void push(int val) override {
        Node* new_node = new Node(val);
        Node* curr_top = top.load(std::memory_order_relaxed);

        while (true) {
            Node* unmarked_top = get_unmarked_ref(curr_top);
            new_node->next.store(unmarked_top, std::memory_order_relaxed);

            if (top.compare_exchange_weak(curr_top, new_node, std::memory_order_release, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    int pop() override {
        Node* curr_top = top.load(std::memory_order_acquire);

        while (true) {
            if (curr_top == nullptr) {
                return INT_MIN;
            }

            Node* unmarked_top = get_unmarked_ref(curr_top);
            if (unmarked_top == nullptr) {
                return INT_MIN;
            }

            Node* next_node = unmarked_top->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_node)) {
                Node* unmarked_next = get_unmarked_ref(next_node);
                if (top.compare_exchange_weak(curr_top, unmarked_next, std::memory_order_release, std::memory_order_acquire)) {
                    continue;
                }
                continue;
            }

            Node* marked_next = get_marked_ref(next_node);
            if (unmarked_top->next.compare_exchange_strong(next_node, marked_next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                int result = unmarked_top->val;
                Node* expected_top = curr_top;
                top.compare_exchange_strong(expected_top, get_unmarked_ref(next_node), std::memory_order_release, std::memory_order_relaxed);
                return result;
            }

            curr_top = top.load(std::memory_order_acquire);
        }
    }

    bool isEmpty() override {
        Node* curr_top = top.load(std::memory_order_acquire);
        while (curr_top != nullptr) {
            Node* unmarked_top = get_unmarked_ref(curr_top);
            if (unmarked_top == nullptr) {
                return true;
            }
            Node* next_node = unmarked_top->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_node)) {
                Node* unmarked_next = get_unmarked_ref(next_node);
                if (top.compare_exchange_weak(curr_top, unmarked_next, std::memory_order_release, std::memory_order_acquire)) {
                    continue;
                }
                curr_top = top.load(std::memory_order_acquire);
            } else {
                return false;
            }
        }
        return true;
    }
};
