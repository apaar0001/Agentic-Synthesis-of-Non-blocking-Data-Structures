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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = head.load(std::memory_order_acquire);
        while (cur != nullptr) {
            Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (t == tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    // try to link new node after t
                    if (t->next.compare_exchange_strong(next, newNode,
                                                      std::memory_order_release, std::memory_order_relaxed)) {
                        // link succeeded, try to advance tail
                        tail.compare_exchange_strong(t, newNode,
                                                   std::memory_order_release, std::memory_order_relaxed);
                        return;
                    }
                    // else retry
                } else {
                    // help advance tail
                    tail.compare_exchange_strong(t, next,
                                               std::memory_order_release, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* t = tail.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(h->next.load(std::memory_order_acquire));
            if (h == head.load(std::memory_order_acquire)) {
                if (h == t) {
                    if (next == nullptr) {
                        return -1; // empty
                    }
                    // tail is falling behind, try to advance it
                    tail.compare_exchange_strong(t, next,
                                               std::memory_order_release, std::memory_order_relaxed);
                } else {
                    Node* first = next; // unmarked
                    int value = first->val;
                    Node* firstNext = get_unmarked_ref(first->next.load(std::memory_order_acquire));
                    // logically delete first by marking its next pointer
                    if (!first->next.compare_exchange_strong(firstNext, get_marked_ref(firstNext),
                                                           std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // failed to mark, help and retry
                        continue;
                    }
                    // try to swing head to the next node
                    if (head.compare_exchange_strong(h, firstNext,
                                                   std::memory_order_release, std::memory_order_relaxed)) {
                        delete first;
                        return value;
                    }
                    // head CAS failed, retry
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* next = get_unmarked_ref(h->next.load(std::memory_order_acquire));
        return next == nullptr;
    }
};