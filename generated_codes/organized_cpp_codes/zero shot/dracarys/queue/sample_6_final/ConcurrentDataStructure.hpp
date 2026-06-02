#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    ConcurrentDataStructure() : head_(new Node(INT_MIN)), tail_(head_) {
        head_->next = get_marked_ref(nullptr);
    }

    ~ConcurrentDataStructure() {
        while (head_) {
            Node* next = get_unmarked_ref(head_->next);
            delete head_;
            head_ = next;
        }
    }

    void enqueue(int val) {
        Node* node = new Node(val);
        while (true) {
            Node* prev_tail = tail_.load(std::memory_order_relaxed);
            Node* prev_next = get_unmarked_ref(prev_tail->next);
            if (prev_tail == tail_.load(std::memory_order_relaxed)) {
                if (prev_next == nullptr) {
                    if (prev_tail->next.compare_exchange_strong(prev_next, node, std::memory_order_acq_rel)) {
                        tail_.compare_exchange_strong(prev_tail, node, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(prev_tail, prev_next, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* prev_head = head_.load(std::memory_order_relaxed);
            Node* prev_tail = tail_.load(std::memory_order_relaxed);
            Node* prev_head_next = get_unmarked_ref(prev_head->next);
            if (prev_head == head_.load(std::memory_order_relaxed)) {
                if (prev_head == prev_tail) {
                    if (prev_head_next == nullptr) {
                        return -1; // empty queue
                    }
                    tail_.compare_exchange_strong(prev_tail, prev_head_next, std::memory_order_relaxed);
                } else {
                    int val = prev_head_next->val;
                    if (head_.compare_exchange_strong(prev_head, prev_head_next, std::memory_order_acq_rel)) {
                        delete prev_head;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head_.load(std::memory_order_relaxed)->next == get_marked_ref(nullptr);
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};