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

    ConcurrentDataStructure() : head(new Node()), tail(new Node()) {
        head->next.store(nullptr, std::memory_order_relaxed);
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node();
        newNode->val = val;
        newNode->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
            if (prevTail == tail.load(std::memory_order_acquire)) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                        tail.compare_exchange_strong(prevTail, newNode, std::memory_order_acq_rel);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* prevHead = head.load(std::memory_order_acquire);
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead == head.load(std::memory_order_acquire)) {
                if (prevHead == prevTail) {
                    if (prevNext == nullptr) {
                        return INT_MIN;
                    }
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                } else {
                    int val = prevNext->val;
                    Node* nextNext = prevNext->next.load(std::memory_order_acquire);
                    if (prevNext->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (prevHead->next.compare_exchange_strong(prevNext, nextNext, std::memory_order_acq_rel)) {
                            delete prevNext;
                            return val;
                        }
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* prevHead = head.load(std::memory_order_acquire);
            Node* prevNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead == head.load(std::memory_order_acquire)) {
                if (prevNext == nullptr) {
                    return true;
                } else if (is_marked_ref(prevNext)) {
                    prevHead->next.compare_exchange_strong(prevNext, get_unmarked_ref(prevNext), std::memory_order_acq_rel);
                } else {
                    return false;
                }
            }
        }
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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};