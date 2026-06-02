#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(head) {}

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = get_unmarked_ref(head->next.load(std::memory_order_relaxed));
            delete head;
            head = next;
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_relaxed);
            Node* prevNext = get_unmarked_ref(prevTail->next.load(std::memory_order_relaxed));
            if (prevTail == tail.load(std::memory_order_relaxed)) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                        tail.compare_exchange_strong(prevTail, newNode, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* prevHead = head.load(std::memory_order_relaxed);
            Node* prevTail = tail.load(std::memory_order_relaxed);
            Node* prevNext = get_unmarked_ref(prevHead->next.load(std::memory_order_relaxed));
            if (prevHead == head.load(std::memory_order_relaxed)) {
                if (prevHead == prevTail) {
                    if (prevNext == nullptr) {
                        return -1; // empty queue
                    }
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_relaxed);
                } else {
                    int val = prevNext->val;
                    if (head.compare_exchange_strong(prevHead, prevNext, std::memory_order_acq_rel)) {
                        delete prevHead;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head.load(std::memory_order_relaxed)->next.load(std::memory_order_relaxed) == nullptr;
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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};