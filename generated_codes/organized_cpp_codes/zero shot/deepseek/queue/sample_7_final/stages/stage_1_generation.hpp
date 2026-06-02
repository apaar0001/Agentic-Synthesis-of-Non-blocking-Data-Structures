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
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* curr_tail = tail.load(std::memory_order_acquire);
            Node* curr_next = curr_tail->next.load(std::memory_order_acquire);
            if (curr_tail == tail.load(std::memory_order_acquire)) {
                if (curr_next == nullptr) {
                    if (curr_tail->next.compare_exchange_strong(curr_next, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(curr_tail, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(curr_tail, curr_next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* curr_head = head.load(std::memory_order_acquire);
            Node* curr_tail = tail.load(std::memory_order_acquire);
            Node* curr_next = curr_head->next.load(std::memory_order_acquire);
            if (curr_head == head.load(std::memory_order_acquire)) {
                if (curr_head == curr_tail) {
                    if (curr_next == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(curr_tail, curr_next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                } else {
                    if (head.compare_exchange_strong(curr_head, curr_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        int val = curr_next->val;
                        delete curr_head;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* curr_head = head.load(std::memory_order_acquire);
        Node* curr_tail = tail.load(std::memory_order_acquire);
        Node* curr_next = curr_head->next.load(std::memory_order_acquire);
        return (curr_head == curr_tail) && (curr_next == nullptr);
    }
};