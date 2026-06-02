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
        head.load(std::memory_order_acquire)->next.store(tail.load(std::memory_order_acquire), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head.load(std::memory_order_acquire)) {
            Node* next = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
            delete head.load(std::memory_order_acquire);
            head.store(next, std::memory_order_relaxed);
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node();
        newNode->val = val;
        newNode->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
            if (prevTail != tail.load(std::memory_order_acquire)) {
                continue;
            }
            if (prevNext != nullptr) {
                tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                continue;
            }
            if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                tail.compare_exchange_strong(prevTail, newNode, std::memory_order_acq_rel);
                return;
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* headNode = head.load(std::memory_order_acquire);
            Node* tailNode = tail.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head.load(std::memory_order_acquire)) {
                if (headNode == tailNode) {
                    if (nextNode == nullptr) {
                        return INT_MIN;
                    }
                    tail.compare_exchange_strong(tailNode, nextNode, std::memory_order_acq_rel);
                } else {
                    int val = nextNode->val;
                    if (nextNode->next.compare_exchange_strong(nullptr, get_marked_ref(nullptr), std::memory_order_acq_rel)) {
                        // Node has been marked
                        head.compare_exchange_strong(headNode, nextNode, std::memory_order_acq_rel);
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        while (true) {
            Node* headNode = head.load(std::memory_order_acquire);
            Node* tailNode = tail.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head.load(std::memory_order_acquire)) {
                if (headNode == tailNode) {
                    return nextNode == nullptr;
                } else {
                    return false;
                }
            }
        }
    }

private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};