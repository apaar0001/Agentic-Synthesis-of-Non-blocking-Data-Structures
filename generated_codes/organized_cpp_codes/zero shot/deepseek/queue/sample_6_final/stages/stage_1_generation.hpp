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
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(INT_MIN);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            if (last == tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (last->next.compare_exchange_strong(next, new_node,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(last, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(last, get_unmarked_ref(next),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            if (first == head.load(std::memory_order_acquire)) {
                if (first == last) {
                    if (next == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(last, get_unmarked_ref(next),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                } else {
                    if (head.compare_exchange_strong(first, get_unmarked_ref(next),
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        int ret = next->val;
                        delete first;
                        return ret;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* first = head.load(std::memory_order_acquire);
        Node* next = first->next.load(std::memory_order_acquire);
        return next == nullptr;
    }
};