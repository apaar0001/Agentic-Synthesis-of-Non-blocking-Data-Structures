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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(head) {
        head->next = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = get_unmarked_ref(head->next);
            delete head;
            head = next;
        }
    }

    void enqueue(int val) {
        Node* node = new Node(val);
        while (true) {
            Node* prevTail = tail;
            Node* prevNext = get_unmarked_ref(prevTail->next);
            if (prevTail == tail) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, node, std::memory_order_acq_rel)) {
                        if (prevTail->next == node) {
                            tail.compare_exchange_strong(prevTail, node, std::memory_order_acq_rel);
                        }
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
            Node* prevHead = head;
            Node* prevTail = tail;
            Node* prevHeadNext = get_unmarked_ref(prevHead->next);
            if (prevHead == head) {
                if (prevHead == prevTail) {
                    if (prevHeadNext == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(prevTail, prevHeadNext, std::memory_order_acq_rel);
                } else {
                    int val = prevHeadNext->val;
                    if (prevHead->next.compare_exchange_strong(prevHeadNext, get_unmarked_ref(prevHeadNext->next), std::memory_order_acq_rel)) {
                        delete prevHead;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head->next == nullptr;
    }

private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};