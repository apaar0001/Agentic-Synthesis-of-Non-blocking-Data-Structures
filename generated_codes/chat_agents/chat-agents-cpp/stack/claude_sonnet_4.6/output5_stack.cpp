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

    // Any thread that sees a marked top helps complete the removal
    void help_finish_pop() {
        Node* t = top.load(std::memory_order_acquire);
        if (!is_marked_ref(t)) return;
        Node* t_clean = get_unmarked_ref(t);
        if (!t_clean) {
            top.compare_exchange_strong(t, nullptr,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return;
        }
        Node* next = get_unmarked_ref(t_clean->next.load(std::memory_order_acquire));
        top.compare_exchange_strong(t, next,
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
            help_finish_pop();
            old_top = get_unmarked_ref(top.load(std::memory_order_acquire));
            node->next.store(old_top, std::memory_order_relaxed);
        } while (!top.compare_exchange_weak(old_top, node,
                    std::memory_order_acq_rel, std::memory_order_acquire));
    }

    int pop() override {
        while (true) {
            help_finish_pop();
            Node* t = get_unmarked_ref(top.load(std::memory_order_acquire));
            if (!t) return INT_MIN;

            // Logically delete: mark the top pointer
            Node* marked = get_marked_ref(t);
            if (!top.compare_exchange_weak(t, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                continue;

            // Physically unlink
            Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            top.compare_exchange_strong(marked, next,
                std::memory_order_acq_rel, std::memory_order_relaxed);

            int res = t->val;
            delete t;
            return res;
        }
    }

    bool isEmpty() override {
        help_finish_pop();
        return get_unmarked_ref(top.load(std::memory_order_acquire)) == nullptr;
    }
};
