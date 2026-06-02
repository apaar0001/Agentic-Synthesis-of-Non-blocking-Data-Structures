#pragma once
#include <atomic>

class QueueDifferentNames {
private:
    struct QNode {
        const int data;
        std::atomic<QNode*> link;

        QNode(int d) : data(d), link(nullptr) {}
    };

    std::atomic<QNode*> firstNode;
    std::atomic<QNode*> lastNode;

public:
    QueueDifferentNames() {
        QNode* s = new QNode(7);
        firstNode.store(s);
        lastNode.store(s);
    }

    void enqueue(int val) {
        QNode* q = new QNode(val);
        while (true) {
            QNode* l = lastNode.load();
            QNode* n = l->link.load();
            if (l == lastNode.load()) {
                if (n == nullptr) {
                    QNode* expected = nullptr;
                    if (l->link.compare_exchange_strong(expected, q)) {
                        lastNode.compare_exchange_strong(l, q);
                        return;
                    }
                } else {
                    lastNode.compare_exchange_strong(l, n);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            QNode* f = firstNode.load();
            QNode* l = lastNode.load();
            QNode* n = f->link.load();
            if (f == firstNode.load()) {
                if (f == l) {
                    if (n == nullptr) return -1;
                    lastNode.compare_exchange_strong(l, n);
                } else {
                    int d = n->data;
                    if (firstNode.compare_exchange_strong(f, n)) {
                        return d;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return firstNode.load()->link.load() == nullptr;
    }
};
