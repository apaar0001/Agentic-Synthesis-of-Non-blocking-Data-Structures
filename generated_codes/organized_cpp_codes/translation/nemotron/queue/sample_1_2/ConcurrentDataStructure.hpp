#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = head.load(std::memory_order_relaxed);
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = nxt;
        }
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        node->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (next == nullptr) {
                Node* expected = nullptr;
                if (t->next.compare_exchange_strong(expected, node,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    tail.compare_exchange_strong(t, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
                    return;
                }
            } else {
                tail.compare_exchange_strong(t, next,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* first = get_unmarked_ref(h->next.load(std::memory_order_acquire));
            if (first == nullptr) {
                return INT_MIN;
            }
            Node* marked_first = get_marked_ref(first);
            if (h->next.compare_exchange_strong(first, marked_first,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
                // Node has been marked
                Node* next = get_unmarked_ref(first->next.load(std::memory_order_acquire));
                if (h->next.compare_exchange_strong(marked_first, next,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    int v = first->val;
                    return v;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* first = get_unmarked_ref(head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire));
        return first == nullptr;
    }
};