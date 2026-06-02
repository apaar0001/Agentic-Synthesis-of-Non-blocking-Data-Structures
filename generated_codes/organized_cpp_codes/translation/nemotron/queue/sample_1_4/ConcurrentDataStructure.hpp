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
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
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
        Node* curr = head.load(std::memory_order_acquire);
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = t->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                Node* unmarkedNext = get_unmarked_ref(next);
                tail.compare_exchange_weak(t, unmarkedNext,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
                continue;
            }
            if (next == nullptr) {
                if (t->next.compare_exchange_weak(next, newNode,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                    tail.compare_exchange_weak(t, newNode,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
                    return;
                }
            } else {
                tail.compare_exchange_weak(t, get_unmarked_ref(next),
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = h->next.load(std::memory_order_acquire);
            if (h == t) {
                if (get_unmarked_ref(next) == nullptr) {
                    return INT_MIN;
                }
                tail.compare_exchange_weak(t, get_unmarked_ref(next),
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire);
                continue;
            }
            if (get_unmarked_ref(next) == nullptr) {
                return INT_MIN;
            }
            Node* markedNext = get_marked_ref(next);
            if (h->next.compare_exchange_weak(next, markedNext,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire)) {
                // Node has been marked
                int val = next->val;
                Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                h->next.compare_exchange_strong(markedNext, succ,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
                return val;
            }
        }
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* next = h->next.load(std::memory_order_acquire);
        return get_unmarked_ref(next) == nullptr;
    }
};