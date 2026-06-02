#pragma once
#include <atomic>

class QueueSentinelNode {
private:
    struct Node {
        const int v;
        std::atomic<Node*> f;

        Node(int val) : v(val), f(nullptr) {}
    };

    std::atomic<Node*> headRef;
    std::atomic<Node*> tailRef;

public:
    QueueSentinelNode() {
        Node* s = new Node(-1);
        headRef.store(s);
        tailRef.store(s);
    }

    void enqueue(int v) {
        Node* n = new Node(v);
        while (true) {
            Node* t = tailRef.load();
            Node* f = t->f.load();
            if (t == tailRef.load()) {
                if (f == nullptr) {
                    Node* expected = nullptr;
                    if (t->f.compare_exchange_strong(expected, n)) {
                        tailRef.compare_exchange_strong(t, n);
                        return;
                    }
                } else {
                    tailRef.compare_exchange_strong(t, f);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* h = headRef.load();
            Node* t = tailRef.load();
            Node* f = h->f.load();
            if (h == headRef.load()) {
                if (h == t) {
                    if (f == nullptr) return -1;
                    tailRef.compare_exchange_strong(t, f);
                } else {
                    int v = f->v;
                    if (headRef.compare_exchange_strong(h, f)) {
                        return v;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return headRef.load()->f.load() == nullptr;
    }
};
