#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    ConcurrentDataStructure() : head_(new Node(INT_MIN)), tail_(head_) {
        head_->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* current = head_;
        while (current) {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail_.load(std::memory_order_acquire);
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
            if (prevTail == tail_.load(std::memory_order_acquire)) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                        tail_.compare_exchange_strong(prevTail, newNode, std::memory_order_acq_rel);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* prevHead = head_.load(std::memory_order_acquire);
            Node* prevTail = tail_.load(std::memory_order_acquire);
            Node* prevNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead == head_.load(std::memory_order_acquire)) {
                if (prevHead == prevTail) {
                    if (prevNext == nullptr) {
                        return -1;
                    }
                    tail_.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                } else {
                    int val = prevNext->val;
                    if (head_.compare_exchange_strong(prevHead, prevNext, std::memory_order_acq_rel)) {
                        delete prevHead;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head_.load(std::memory_order_acquire)->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val) {}
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