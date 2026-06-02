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

    void help_remove(Node* marked_node, Node* expected_next) {
        Node* unmarked = get_unmarked_ref(expected_next);
        top.compare_exchange_strong(marked_node, unmarked,
            std::memory_order_acq_rel, std::memory_order_relaxed);
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
            old_top = get_unmarked_ref(top.load(std::memory_order_acquire));
            node->next.store(old_top, std::memory_order_relaxed);
        } while (!top.compare_exchange_weak(old_top, node,
                    std::memory_order_acq_rel, std::memory_order_acquire));
    }

    int pop() override {
        while (true) {
            Node* t = top.load(std::memory_order_acquire);
            Node* t_unmarked = get_unmarked_ref(t);
            if (!t_unmarked) return INT_MIN;

            Node* next = t_unmarked->next.load(std::memory_order_acquire);

            // Phase 1: logical mark
            Node* next_unmarked = get_unmarked_ref(next);
            Node* next_marked   = get_marked_ref(next_unmarked);
            if (!t_unmarked->next.compare_exchange_strong(next_unmarked, next_marked,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;

            // Phase 2: physical unlink
            help_remove(t_unmarked, next_marked);
            int result = t_unmarked->val;
            delete t_unmarked;
            return result;
        }
    }

    bool isEmpty() override {
        return get_unmarked_ref(top.load(std::memory_order_acquire)) == nullptr;
    }
};
