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

    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    void help_advance_tail(Node* last, Node* next) {
        tail_.compare_exchange_strong(last, next, std::memory_order_acq_rel, std::memory_order_relaxed);
    }

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
        Node* last, *next;
        while (true) {
            last = tail_.load(std::memory_order_acquire);
            next = get_unmarked_ref(last)->next.load(std::memory_order_acquire);
            if (last != tail_.load(std::memory_order_acquire)) continue;
            if (get_unmarked_ref(next) != nullptr) { help_advance_tail(last, get_unmarked_ref(next)); continue; }
            Node* exp = nullptr;
            if (get_unmarked_ref(last)->next.compare_exchange_strong(exp, nd, std::memory_order_acq_rel)) {
                help_advance_tail(last, nd);
                return;
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last  = tail_.load(std::memory_order_acquire);
            Node* firstRaw = get_unmarked_ref(first);
            Node* next = firstRaw->next.load(std::memory_order_acquire);
            Node* nextRaw = get_unmarked_ref(next);
            if (first != head_.load(std::memory_order_acquire)) continue;
            if (firstRaw == get_unmarked_ref(last)) {
                if (nextRaw == nullptr) return INT_MIN;
                help_advance_tail(last, nextRaw);
            } else {
                int v = nextRaw->val;
                if (head_.compare_exchange_strong(first, nextRaw, std::memory_order_acq_rel)) {
                    delete firstRaw;
                    return v;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = get_unmarked_ref(head_.load(std::memory_order_acquire));
        Node* t = get_unmarked_ref(tail_.load(std::memory_order_acquire));
        return h == t && get_unmarked_ref(h->next.load(std::memory_order_acquire)) == nullptr;
    }
};
