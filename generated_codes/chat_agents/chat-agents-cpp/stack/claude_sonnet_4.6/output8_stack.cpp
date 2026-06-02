#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public StackADT {
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
        Node* cur = get_unmarked_ref(top.load(std::memory_order_relaxed));
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = nxt;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);
        Node* old_top;
        do {
            old_top = top.load(std::memory_order_acquire);
            node->next.store(old_top, std::memory_order_relaxed);
        } while (!top.compare_exchange_weak(old_top, node,
                    std::memory_order_acq_rel, std::memory_order_acquire));
    }

    int pop() override {
        Node* old_top;
        Node* next_node;
        do {
            old_top = top.load(std::memory_order_acquire);
            if (!old_top) return INT_MIN;
            next_node = old_top->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_node)) {
                top.compare_exchange_weak(old_top,
                    get_unmarked_ref(next_node),
                    std::memory_order_acq_rel, std::memory_order_acquire);
                continue;
            }
        } while (!top.compare_exchange_weak(old_top, next_node,
                    std::memory_order_acq_rel, std::memory_order_acquire));
        int result = old_top->val;
        delete old_top;
        return result;
    }

    bool isEmpty() override {
        Node* t = top.load(std::memory_order_acquire);
        return get_unmarked_ref(t) == nullptr;
    }
};
