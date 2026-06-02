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

        Node(int val) : val(val), next(nullptr) {}
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(head) {
        head->next = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* next = get_unmarked_ref(head->next.load(std::memory_order_relaxed));
            delete head;
            head = next;
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_relaxed);
            Node* prevNext = get_unmarked_ref(prevTail->next.load(std::memory_order_acquire));
            if (prevTail == tail.load(std::memory_order_relaxed)) {
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

    int dequeue() {
        while (true) {
            Node* prevHead = head.load(std::memory_order_relaxed);
            Node* prevTail = tail.load(std::memory_order_relaxed);
            Node* prevHeadNext = get_unmarked_ref(prevHead->next.load(std::memory_order_acquire));

            if (prevHead == head.load(std::memory_order_relaxed)) {
                if (prevHead == prevTail) {
                    if (prevHeadNext == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(prevTail, prevHeadNext, std::memory_order_acq_rel);
                } else {
                    int val = prevHeadNext->val;
                    if (head.compare_exchange_strong(prevHead, prevHeadNext, std::memory_order_acq_rel)) {
                        delete prevHead;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        Node* prevHead = head.load(std::memory_order_relaxed);
        Node* prevHeadNext = get_unmarked_ref(prevHead->next.load(std::memory_order_acquire));
        return prevHead == tail.load(std::memory_order_relaxed) && prevHeadNext == nullptr;
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

private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};