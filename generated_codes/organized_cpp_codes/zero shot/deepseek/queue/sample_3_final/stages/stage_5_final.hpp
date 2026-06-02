#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

struct Node {
    int val;
    std::atomic<Node*> next;
    Node(int v) : val(v), next(nullptr) {}
};

class ConcurrentDataStructure : public QueueADT {
private:
    alignas(64) std::atomic<Node*> head;
    alignas(64) std::atomic<Node*> tail;

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
            Node* next = curr->next.load(std::memory_order_relaxed);
            next = get_unmarked_ref(next);
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
                    Node* unmarked_next = nullptr;
                    if (last->next.compare_exchange_strong(unmarked_next, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(last, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(last, next,
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
                    tail.compare_exchange_strong(last, next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                } else {
                    if (next == nullptr) continue;
                    int retval = next->val;
                    if (head.compare_exchange_strong(first, next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        delete first;
                        return retval;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* first = head.load(std::memory_order_acquire);
        Node* last = tail.load(std::memory_order_acquire);
        Node* next = first->next.load(std::memory_order_acquire);
        return (first == last && next == nullptr);
    }
};