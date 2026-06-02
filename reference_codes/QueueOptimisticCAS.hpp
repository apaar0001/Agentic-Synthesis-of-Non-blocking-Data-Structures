#pragma once
#include <atomic>

class QueueOptimisticCAS {
private:
    struct Node {
        const int val;
        std::atomic<Node*> next;

        Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> h;
    std::atomic<Node*> t;

public:
    QueueOptimisticCAS() {
        Node* n = new Node(0);
        h.store(n);
        t.store(n);
    }

    void enqueue(int v) {
        Node* n = new Node(v);
        while (true) {
            Node* curT = t.load();
            Node* next = curT->next.load();
            if (next == nullptr) {
                Node* expected = nullptr;
                if (curT->next.compare_exchange_strong(expected, n)) {
                    t.compare_exchange_strong(curT, n);
                    return;
                }
            } else {
                t.compare_exchange_strong(curT, next);
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* curH = h.load();
            Node* curT = t.load();
            Node* next = curH->next.load();
            if (curH == curT) {
                if (next == nullptr) return -1;
                t.compare_exchange_strong(curT, next);
            } else {
                int res = next->val;
                if (h.compare_exchange_strong(curH, next)) {
                    return res;
                }
            }
        }
    }

    bool isEmpty() {
        return h.load()->next.load() == nullptr;
    }
};
