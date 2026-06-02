#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <thread>
#include <chrono>

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

    static void backoff(int& delay) {
        for (int i = 0; i < delay; ++i)
            std::atomic_signal_fence(std::memory_order_seq_cst);
        if (delay < 1024) delay *= 2;
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
        int delay = 1;
        Node* old_top;
        do {
            old_top = top.load(std::memory_order_acquire);
            node->next.store(get_unmarked_ref(old_top), std::memory_order_relaxed);
            if (!top.compare_exchange_weak(old_top, node,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                backoff(delay);
            else break;
        } while (true);
    }

    int pop() override {
        int delay = 1;
        while (true) {
            Node* t = get_unmarked_ref(top.load(std::memory_order_acquire));
            if (!t) return INT_MIN;
            Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (top.compare_exchange_weak(t, next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                int res = t->val;
                delete t;
                return res;
            }
            backoff(delay);
        }
    }

    bool isEmpty() override {
        return get_unmarked_ref(top.load(std::memory_order_acquire)) == nullptr;
    }
};
