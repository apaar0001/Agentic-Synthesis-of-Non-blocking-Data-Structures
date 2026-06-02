#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(head) {
        head->next = get_marked_ref(nullptr);
    }

    ~ConcurrentDataStructure() {
        Node* node = head;
        while (node) {
            Node* next = get_unmarked_ref(node->next);
            delete node;
            node = next;
        }
    }

    void enqueue(int val) {
        Node* node = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = get_unmarked_ref(prevTail->next);
            if (prevTail == tail.load(std::memory_order_acquire)) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, node, std::memory_order_acq_rel)) {
                        tail.compare_exchange_strong(prevTail, node, std::memory_order_acq_rel);
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
            Node* headNode = head.load(std::memory_order_acquire);
            Node* tailNode = tail.load(std::memory_order_acquire);
            Node* nextNode = get_unmarked_ref(headNode->next);
            if (headNode == head.load(std::memory_order_acquire)) {
                if (headNode == tailNode) {
                    if (nextNode == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(tailNode, nextNode, std::memory_order_acq_rel);
                } else {
                    int value = nextNode->val;
                    if (headNode->next.compare_exchange_strong(nextNode, get_marked_ref(nextNode), std::memory_order_acq_rel)) {
                        head.compare_exchange_strong(headNode, nextNode, std::memory_order_acq_rel);
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        Node* headNode = head.load(std::memory_order_acquire);
        Node* tailNode = tail.load(std::memory_order_acquire);
        return headNode == tailNode && get_unmarked_ref(headNode->next) == nullptr;
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