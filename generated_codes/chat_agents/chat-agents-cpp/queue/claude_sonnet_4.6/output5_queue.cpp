#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

public:
    ConcurrentDataStructure() {
        Node* s = new Node(0);
        head_.store(s, std::memory_order_relaxed);
        tail_.store(s, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* c = get_unmarked_ref(head_.load(std::memory_order_relaxed));
        while (c) { Node* n = get_unmarked_ref(c->next.load(std::memory_order_relaxed)); delete c; c = n; }
    }

    void enqueue(int val) override {
        Node* nd = new Node(val);
        Node* prev = tail_.load(std::memory_order_relaxed);
        Node* last_try = prev;
        while (true) {
            Node* next = prev->next.load(std::memory_order_acquire);
            if (next == nullptr) {
                Node* exp = nullptr;
                if (prev->next.compare_exchange_weak(exp, nd, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    tail_.compare_exchange_strong(last_try, nd, std::memory_order_release, std::memory_order_relaxed);
                    return;
                }
            } else {
                prev = next;
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* next  = first->next.load(std::memory_order_acquire);
            if (next == nullptr) return INT_MIN;
            int v = next->val;
            if (head_.compare_exchange_strong(first, next, std::memory_order_acq_rel)) {
                delete first;
                return v;
            }
        }
    }

    bool isEmpty() override {
        Node* h = head_.load(std::memory_order_acquire);
        return h->next.load(std::memory_order_acquire) == nullptr;
    }
};
