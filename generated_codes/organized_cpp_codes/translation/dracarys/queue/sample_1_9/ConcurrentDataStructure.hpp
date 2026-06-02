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
    };

    ConcurrentDataStructure() : head_(new Node()), tail_(head_.load(std::memory_order_acquire)) {}

    ~ConcurrentDataStructure() {
        while (true) {
            Node* headNode = head_.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head_.load(std::memory_order_acquire)) {
                if (nextNode == nullptr) {
                    delete headNode;
                    return;
                }
                if (is_marked_ref(nextNode)) {
                    continue;
                }
                Node* nextNext = nextNode->next.load(std::memory_order_acquire);
                if (headNode->next.compare_exchange_strong(nextNode, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (nextNext == nullptr) {
                        tail_.compare_exchange_strong(nextNode, headNode, std::memory_order_acq_rel);
                    }
                    delete nextNode;
                }
            }
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node();
        newNode->val = val;
        newNode->next.store(nullptr, std::memory_order_relaxed);
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
                    if (is_marked_ref(prevNext)) {
                        continue;
                    }
                    tail_.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* headNode = head_.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head_.load(std::memory_order_acquire)) {
                if (nextNode == nullptr) {
                    return INT_MIN;
                }
                if (is_marked_ref(nextNode)) {
                    continue;
                }
                Node* nextNext = nextNode->next.load(std::memory_order_acquire);
                if (headNode->next.compare_exchange_strong(nextNode, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                    // Node has been marked
                    int val = nextNode->val;
                    if (nextNext == nullptr) {
                        tail_.compare_exchange_strong(nextNode, headNode, std::memory_order_acq_rel);
                    }
                    delete nextNode;
                    return val;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* headNode = head_.load(std::memory_order_acquire);
        Node* nextNode = headNode->next.load(std::memory_order_acquire);
        return nextNode == nullptr || is_marked_ref(nextNode);
    }

private:
    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
};