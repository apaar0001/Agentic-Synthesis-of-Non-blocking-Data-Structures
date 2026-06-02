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
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = head->next.load(std::memory_order_relaxed);
            delete head;
            head = next;
        }
    }

    void enqueue(int val) {
        Node* newNode = new Node();
        newNode->val = val;
        newNode->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* prevTail = tail;
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
            if (prevTail == tail) {
                if (prevNext == nullptr) {
                    if (prevTail->next.compare_exchange_strong(prevNext, newNode, std::memory_order_acq_rel)) {
                        tail = newNode;
                        return;
                    }
                } else {
                    tail = prevNext;
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* headNode = head;
            Node* tailNode = tail;
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head) {
                if (headNode == tailNode) {
                    if (nextNode == nullptr) {
                        return INT_MIN; // Queue is empty
                    }
                    tail = nextNode;
                } else {
                    int val = nextNode->val;
                    Node* markedNext = get_marked_ref(nextNode->next.load(std::memory_order_acquire));
                    if (nextNode->next.compare_exchange_strong(nextNode->next.load(std::memory_order_acquire), markedNext, std::memory_order_acq_rel)) {
                        // Node has been marked
                        head = nextNode;
                        delete headNode;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        Node* headNode = head;
        Node* nextNode = headNode->next.load(std::memory_order_acquire);
        return headNode == tail && nextNode == nullptr;
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

    Node* head;
    Node* tail;
};