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

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MIN)) {
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* temp = head;
            head = get_unmarked_ref(head->next.load(std::memory_order_acquire));
            delete temp;
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = get_unmarked_ref(prevTail->next.load(std::memory_order_acquire));
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

    int dequeue() {
        while (true) {
            Node* prevHead = head.load(std::memory_order_acquire);
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = get_unmarked_ref(prevHead->next.load(std::memory_order_acquire));
            if (prevHead == head.load(std::memory_order_acquire)) {
                if (prevHead == prevTail) {
                    if (prevNext == nullptr) {
                        return INT_MIN;
                    }
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_acq_rel);
                } else {
                    int val = prevNext->val;
                    Node* markedNext = get_marked_ref(prevNext->next.load(std::memory_order_acquire));
                    if (prevNext->next.compare_exchange_strong(prevNext->next.load(std::memory_order_acquire), markedNext, std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (prevHead->next.compare_exchange_strong(prevNext, get_unmarked_ref(markedNext), std::memory_order_acq_rel)) {
                            return val;
                        }
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

private:
    Node* head;
    Node* tail;
};