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
        Node* sentinel = new Node(0);
        head_.store(sentinel, std::memory_order_relaxed);
        tail_.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = get_unmarked_ref(head_.load(std::memory_order_relaxed));
        while (cur) {
            Node* next = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = next;
        }
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        while (true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            if (last == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_strong(next, node, std::memory_order_acq_rel))
                    { tail_.compare_exchange_strong(last, node, std::memory_order_acq_rel); return; }
                } else {
                    tail_.compare_exchange_strong(last, next, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last  = tail_.load(std::memory_order_acquire);
            Node* next  = first->next.load(std::memory_order_acquire);
            if (first == head_.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) return INT_MIN;
                    tail_.compare_exchange_strong(last, next, std::memory_order_acq_rel);
                } else {
                    int val = next->val;
                    if (head_.compare_exchange_strong(first, next, std::memory_order_acq_rel))
                    { delete first; return val; }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* first = head_.load(std::memory_order_acquire);
        Node* last  = tail_.load(std::memory_order_acquire);
        Node* next  = first->next.load(std::memory_order_acquire);
        return (first == last) && (next == nullptr);
    }
};
