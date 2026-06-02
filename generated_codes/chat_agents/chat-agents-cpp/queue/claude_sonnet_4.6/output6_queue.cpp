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
    std::atomic<int> size_;

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

public:
    ConcurrentDataStructure() : size_(0) {
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
        while (true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(last->next.load(std::memory_order_acquire));
            if (last != tail_.load(std::memory_order_acquire)) continue;
            if (next != nullptr) { tail_.compare_exchange_strong(last, next, std::memory_order_acq_rel); continue; }
            Node* exp = nullptr;
            if (last->next.compare_exchange_strong(exp, nd, std::memory_order_acq_rel)) {
                tail_.compare_exchange_strong(last, nd, std::memory_order_acq_rel);
                size_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last  = tail_.load(std::memory_order_acquire);
            Node* next  = get_unmarked_ref(first->next.load(std::memory_order_acquire));
            if (first != head_.load(std::memory_order_acquire)) continue;
            if (first == last) {
                if (next == nullptr) return INT_MIN;
                tail_.compare_exchange_strong(last, next, std::memory_order_acq_rel);
            } else {
                int v = next->val;
                if (head_.compare_exchange_strong(first, next, std::memory_order_acq_rel)) {
                    size_.fetch_sub(1, std::memory_order_relaxed);
                    delete first;
                    return v;
                }
            }
        }
    }

    bool isEmpty() override {
        return size_.load(std::memory_order_acquire) == 0;
    }
};
