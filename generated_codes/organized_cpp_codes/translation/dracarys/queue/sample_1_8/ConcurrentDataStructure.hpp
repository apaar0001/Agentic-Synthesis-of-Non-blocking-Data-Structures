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
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = next;
        }
    }

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    void enqueue(int val) override {
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

    int dequeue() override {
        while (true) {
            Node* prevHead = head.load(std::memory_order_acquire);
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevHeadNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead == head.load(std::memory_order_acquire)) {
                if (prevHead == prevTail) {
                    if (prevHeadNext == nullptr) {
                        return INT_MIN; // Queue is empty
                    }
                    tail.compare_exchange_strong(prevTail, prevHeadNext, std::memory_order_acq_rel);
                } else {
                    Node* markedNext = get_marked_ref(prevHeadNext);
                    if (prevHead->next.compare_exchange_strong(prevHeadNext, markedNext, std::memory_order_acq_rel)) {
                        // Node has been marked
                        Node* nextNext = prevHeadNext->next.load(std::memory_order_acquire);
                        if (prevHead->next.compare_exchange_strong(markedNext, nextNext, std::memory_order_acq_rel)) {
                            delete prevHeadNext;
                        }
                        head.compare_exchange_strong(prevHead, nextNext, std::memory_order_acq_rel);
                        return prevHeadNext->val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* prevHead = head.load(std::memory_order_acquire);
            Node* prevHeadNext = prevHead->next.load(std::memory_order_acquire);
            if (prevHead == head.load(std::memory_order_acquire)) {
                if (prevHeadNext == nullptr) {
                    return true;
                }
                if (!is_marked_ref(prevHeadNext)) {
                    return false;
                }
                Node* nextNext = get_unmarked_ref(prevHeadNext)->next.load(std::memory_order_acquire);
                if (prevHead->next.compare_exchange_strong(prevHeadNext, nextNext, std::memory_order_acq_rel)) {
                    delete get_unmarked_ref(prevHeadNext);
                }
            }
        }
    }

private:
    Node* headNode;
    Node* tailNode;
    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    ConcurrentDataStructure() : headNode(new Node()), tailNode(new Node()), head(headNode), tail(tailNode) {
        headNode->next.store(tailNode, std::memory_order_relaxed);
        tailNode->next.store(nullptr, std::memory_order_relaxed);
    }
};