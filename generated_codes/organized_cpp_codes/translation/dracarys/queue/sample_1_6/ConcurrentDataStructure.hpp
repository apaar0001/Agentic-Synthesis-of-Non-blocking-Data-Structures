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

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    ConcurrentDataStructure() : head(new Node()), tail(new Node()) {
        head.load(std::memory_order_acquire)->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head.load(std::memory_order_acquire)) {
            Node* next = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
            delete head.load(std::memory_order_acquire);
            head.store(next, std::memory_order_relaxed);
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
                    if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        tail.compare_exchange_strong(prevTail, newNode, std::memory_order_acq_rel, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* headNode = head.load(std::memory_order_acquire);
            Node* tailNode = tail.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);

            if (headNode == head.load(std::memory_order_acquire)) {
                if (headNode == tailNode) {
                    if (nextNode == nullptr) {
                        return INT_MIN;
                    }
                    tail.compare_exchange_strong(tailNode, nextNode, std::memory_order_acq_rel, std::memory_order_relaxed);
                } else {
                    if (is_marked_ref(nextNode)) {
                        continue;
                    }
                    int val = nextNode->val;
                    if (headNode->next.compare_exchange_strong(nextNode, get_marked_ref(nextNode), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        // Node has been marked
                        head.compare_exchange_strong(headNode, get_unmarked_ref(nextNode), std::memory_order_acq_rel, std::memory_order_relaxed);
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* headNode = head.load(std::memory_order_acquire);
        Node* nextNode = headNode->next.load(std::memory_order_acquire);
        return nextNode == nullptr || is_marked_ref(nextNode);
    }

private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};