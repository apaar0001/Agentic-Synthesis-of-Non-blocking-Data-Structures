#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        char pad[64 - sizeof(int) - sizeof(std::atomic<Node*>)];
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    struct alignas(64) PaddedAtomic {
        std::atomic<Node*> ptr;
        char pad[64 - sizeof(std::atomic<Node*>)];
    };

    PaddedAtomic head_;
    PaddedAtomic tail_;

public:
    ConcurrentDataStructure() {
        Node* s = new Node(0);
        head_.ptr.store(s, std::memory_order_relaxed);
        tail_.ptr.store(s, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* c = get_unmarked_ref(head_.ptr.load(std::memory_order_relaxed));
        while (c) { Node* n = get_unmarked_ref(c->next.load(std::memory_order_relaxed)); delete c; c = n; }
    }

    void enqueue(int val) override {
        Node* nd = new Node(val);
        while (true) {
            Node* last = tail_.ptr.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            if (last == tail_.ptr.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_strong(next, nd, std::memory_order_acq_rel)) {
                        tail_.ptr.compare_exchange_strong(last, nd, std::memory_order_acq_rel);
                        return;
                    }
                } else {
                    tail_.ptr.compare_exchange_strong(last, next, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head_.ptr.load(std::memory_order_acquire);
            Node* last  = tail_.ptr.load(std::memory_order_acquire);
            Node* next  = first->next.load(std::memory_order_acquire);
            if (first == head_.ptr.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) return INT_MIN;
                    tail_.ptr.compare_exchange_strong(last, next, std::memory_order_acq_rel);
                } else {
                    int v = next->val;
                    if (head_.ptr.compare_exchange_strong(first, next, std::memory_order_acq_rel)) {
                        delete first;
                        return v;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = head_.ptr.load(std::memory_order_acquire);
        Node* t = tail_.ptr.load(std::memory_order_acquire);
        return h == t && h->next.load(std::memory_order_acquire) == nullptr;
    }
};
