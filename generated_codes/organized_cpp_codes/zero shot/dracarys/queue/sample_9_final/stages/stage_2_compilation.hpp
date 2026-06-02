#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    ConcurrentDataStructure() : head_(new Node{INT_MIN, nullptr}), tail_(head_) {
        head_->next = new Node{INT_MAX, nullptr};
    }

    ~ConcurrentDataStructure() {
        while (true) {
            Node* node = head_.load(std::memory_order_relaxed);
            Node* next = node->next.load(std::memory_order_relaxed);
            if (node == head_.load(std::memory_order_relaxed)) {
                if (get_unmarked_ref(next) == node) {
                    break;
                }
                delete node;
                head_.store(get_unmarked_ref(next), std::memory_order_relaxed);
            }
        }
        delete get_unmarked_ref(head_.load(std::memory_order_relaxed));
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    void enqueue(int val) {
        Node* node = new Node{val, nullptr};
        while (true) {
            Node* prev = tail_.load(std::memory_order_relaxed);
            Node* next = prev->next.load(std::memory_order_relaxed);
            if (prev == tail_.load(std::memory_order_relaxed)) {
                if (get_unmarked_ref(next) == prev) {
                    if (next->next.compare_exchange_strong(nullptr, node, std::memory_order_acq_rel)) {
                        prev->next.compare_exchange_strong(next, node, std::memory_order_acq_rel);
                        tail_.compare_exchange_strong(prev, node, std::memory_order_acq_rel);
                        break;
                    }
                } else {
                    tail_.compare_exchange_strong(prev, get_unmarked_ref(next), std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* head = head_.load(std::memory_order_relaxed);
            Node* tail = tail_.load(std::memory_order_relaxed);
            Node* next = head->next.load(std::memory_order_relaxed);
            if (head == head_.load(std::memory_order_relaxed)) {
                if (head == tail) {
                    if (get_unmarked_ref(next) == head) {
                        return -1;
                    }
                    tail_.compare_exchange_strong(tail, get_unmarked_ref(next), std::memory_order_relaxed);
                } else {
                    int val = next->val;
                    if (head_->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                        head_.compare_exchange_strong(head, get_unmarked_ref(next), std::memory_order_acq_rel);
                        delete get_unmarked_ref(next);
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head_.load(std::memory_order_relaxed)->next.load(std::memory_order_relaxed) == head_.load(std::memory_order_relaxed);
    }

private:
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};