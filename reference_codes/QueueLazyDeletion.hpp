#pragma once
#include <atomic>

class QueueLazyDeletion {
private:
    struct Node {
        const int v;
        std::atomic<Node*> n;
        std::atomic<bool> d;

        Node(int val) : v(val), n(nullptr), d(false) {}
    };

    std::atomic<Node*> h;
    std::atomic<Node*> t;

public:
    QueueLazyDeletion() {
        Node* s = new Node(0);
        s->d.store(true);
        h.store(s);
        t.store(s);
    }

    void enqueue(int v) {
        Node* nn = new Node(v);
        while (true) {
            Node* la = t.load();
            Node* ne = la->n.load();
            if (la == t.load()) {
                if (ne == nullptr) {
                    Node* expected = nullptr;
                    if (la->n.compare_exchange_strong(expected, nn)) {
                        t.compare_exchange_strong(la, nn);
                        return;
                    }
                } else {
                    t.compare_exchange_strong(la, ne);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* fi = h.load();
            Node* la = t.load();
            Node* ne = fi->n.load();
            if (fi == h.load()) {
                if (fi == la) {
                    if (ne == nullptr) return -1;
                    t.compare_exchange_strong(la, ne);
                } else {
                    int val = ne->v;
                    bool expected = false;
                    if (ne->d.compare_exchange_strong(expected, true)) {
                        h.compare_exchange_strong(fi, ne);
                        return val;
                    }
                    h.compare_exchange_strong(fi, ne);
                }
            }
        }
    }

    bool isEmpty() {
        return h.load()->n.load() == nullptr;
    }
};
