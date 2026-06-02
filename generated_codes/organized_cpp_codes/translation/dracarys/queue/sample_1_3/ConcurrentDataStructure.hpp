#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/QueueADT.hpp"

class ConcurrentDataStructure : public QueueADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int val) : val(val), next(nullptr) {}
    };

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    ConcurrentDataStructure() : head_(new Node(INT_MIN)) {
        tail_.store(head_.load(std::memory_order_acquire));
    }

    ~ConcurrentDataStructure() {
        while (head_.load(std::memory_order_acquire)) {
            Node* next = head_.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
            delete head_.load(std::memory_order_acquire);
            head_.store(next, std::memory_order_release);
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail_.load(std::memory_order_acquire);
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
            if (prevTail != tail_.load(std::memory_order_acquire)) {
                continue;
            }
            if (prevNext != nullptr) {
                tail_.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                continue;
            }
            if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                tail_.compare_exchange_strong(prevTail, newNode, std::memory_order_acq_rel);
                return;
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* prevHead = head_.load(std::memory_order_acquire);
            Node* prevNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead != head_.load(std::memory_order_acquire)) {
                continue;
            }
            if (prevNext == nullptr) {
                return INT_MIN;
            }
            if (is_marked_ref(prevNext)) {
                continue;
            }
            if (prevHead->next.compare_exchange_strong(prevNext, get_marked_ref(prevNext), std::memory_order_acq_rel)) {
                // Node has been marked
                int val = prevNext->val;
                Node* next = prevNext->next.load(std::memory_order_acquire);
                if (prevHead->next.compare_exchange_strong(get_marked_ref(prevNext), next, std::memory_order_acq_rel)) {
                    return val;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* next = head_.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        return next == nullptr || is_marked_ref(next);
    }

private:
    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};