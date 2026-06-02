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

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
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
        Node* cur = head.load(std::memory_order_acquire);
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = nxt;
        }
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        node->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = t->next.load(std::memory_order_acquire);
            if (next != nullptr) {
                Node* unmarked_next = get_unmarked_ref(next);
                tail.compare_exchange_weak(t, unmarked_next,
                    std::memory_order_release, std::memory_order_relaxed);
                continue;
            }
            if (t->next.compare_exchange_weak(next, node,
                    std::memory_order_release, std::memory_order_relaxed)) {
                tail.compare_exchange_weak(t, node,
                    std::memory_order_release, std::memory_order_relaxed);
                return;
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* first = h->next.load(std::memory_order_acquire);

            if (is_marked_ref(first)) {
                Node* second = get_unmarked_ref(first->next.load(std::memory_order_acquire));
                head.compare_exchange_weak(h, second,
                    std::memory_order_release, std::memory_order_relaxed);
                continue;
            }

            if (first == nullptr) {
                return INT_MIN;
            }

            Node* marked_first = get_marked_ref(first);
            if (h->next.compare_exchange_weak(first, marked_first,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                Node* second = get_unmarked_ref(first->next.load(std::memory_order_acquire));
                head.compare_exchange_weak(h, second,
                    std::memory_order_release, std::memory_order_relaxed);
                return first->val;
            }
        }
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* curr = h->next.load(std::memory_order_acquire);
        while (curr != nullptr && is_marked_ref(curr)) {
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
        return curr == nullptr;
    }
};