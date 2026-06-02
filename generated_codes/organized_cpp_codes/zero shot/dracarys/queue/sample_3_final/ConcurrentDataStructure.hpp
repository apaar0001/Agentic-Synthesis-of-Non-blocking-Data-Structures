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
        while (true) {
            Node* node = head.load(std::memory_order_relaxed);
            Node* next = get_unmarked_ref(node->next.load(std::memory_order_relaxed));
            if (node == head.load(std::memory_order_relaxed)) {
                if (next == nullptr) break;
                delete node;
                head.store(next, std::memory_order_relaxed);
            }
        }
        delete head;
    }

    void enqueue(int val) {
        Node* node = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_relaxed);
            Node* prevNext = get_unmarked_ref(prevTail->next.load(std::memory_order_relaxed));
            if (prevTail == tail.load(std::memory_order_relaxed)) {
                if (prevNext != nullptr) {
                    tail.compare_exchange_strong(prevTail, prevNext, std::memory_order_relaxed);
                } else {
                    if (prevTail->next.compare_exchange_strong(prevNext, node, std::memory_order_acq_rel)) {
                        tail.compare_exchange_strong(prevTail, node, std::memory_order_relaxed);
                        return;
                    }
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* headNode = head.load(std::memory_order_relaxed);
            Node* tailNode = tail.load(std::memory_order_relaxed);
            Node* next = get_unmarked_ref(headNode->next.load(std::memory_order_relaxed));
            if (headNode == head.load(std::memory_order_relaxed)) {
                if (headNode == tailNode) {
                    if (next == nullptr) return -1;
                    tail.compare_exchange_strong(tailNode, next, std::memory_order_relaxed);
                } else {
                    int val = next->val;
                    if (head.compare_exchange_strong(headNode, next, std::memory_order_acq_rel)) {
                        delete headNode;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        Node* headNode = head.load(std::memory_order_relaxed);
        Node* next = get_unmarked_ref(headNode->next.load(std::memory_order_relaxed));
        return next == nullptr;
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