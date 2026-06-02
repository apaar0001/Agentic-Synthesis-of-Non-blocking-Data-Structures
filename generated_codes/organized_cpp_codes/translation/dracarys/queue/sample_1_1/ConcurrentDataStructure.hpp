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

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MIN)) {
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = head->next.load(std::memory_order_relaxed);
            delete head;
            head = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* prevTail = tail.load(std::memory_order_acquire);
            Node* prevNext = prevTail->next.load(std::memory_order_acquire);
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

    int dequeue() override {
        while (true) {
            Node* headNode = head.load(std::memory_order_acquire);
            Node* tailNode = tail.load(std::memory_order_acquire);
            Node* nextNode = headNode->next.load(std::memory_order_acquire);
            if (headNode == head.load(std::memory_order_acquire)) {
                if (headNode == tailNode) {
                    if (nextNode == nullptr) {
                        return INT_MIN; // Queue is empty
                    }
                    tail.compare_exchange_strong(tailNode, nextNode, std::memory_order_acq_rel);
                } else {
                    int val = nextNode->val;
                    Node* nextNext = nextNode->next.load(std::memory_order_acquire);
                    if (nextNext == nullptr) {
                        return val;
                    }
                    if (headNode->next.compare_exchange_strong(nextNode, nextNext, std::memory_order_acq_rel)) {
                        // Node has been marked
                        delete nextNode;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* headNode = head.load(std::memory_order_acquire);
        Node* nextNode = headNode->next.load(std::memory_order_acquire);
        return nextNode == nullptr;
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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};