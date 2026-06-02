#pragma once
#include <atomic>

class ConcurrentDataStructure {
private:
    struct Node {
        const int value;
        std::atomic<Node*> next;

        Node(int val) : value(val), next(nullptr) {}
    };

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(0);
        head.store(sentinel);
        tail.store(sentinel);
    }

    void enqueue(int val) {
        Node* newNode = new Node(val);
        while (true) {
            Node* last = tail.load();
            Node* next = last->next.load();
            if (last == tail.load()) {
                if (next == nullptr) {
                    Node* expected = nullptr;
                    if (last->next.compare_exchange_strong(expected, newNode)) {
                        tail.compare_exchange_strong(last, newNode);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(last, next);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* first = head.load();
            Node* last = tail.load();
            Node* next = first->next.load();
            if (first == head.load()) {
                if (first == last) {
                    if (next == nullptr) return -1;
                    tail.compare_exchange_strong(last, next);
                } else {
                    int value = next->value;
                    if (head.compare_exchange_strong(first, next)) {
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head.load()->next.load() == nullptr;
    }
};
