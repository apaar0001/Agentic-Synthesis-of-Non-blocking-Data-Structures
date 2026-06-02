#pragma once
#include <atomic>
#include <limits>

class BSTOptimisticCAS {
private:
    struct Node {
        const int k; // key
        std::atomic<bool> m{false}; // marked/deleted
        std::atomic<Node*> l{nullptr}; // left
        std::atomic<Node*> r{nullptr}; // right

        Node(int key) : k(key) {}
    };

    std::atomic<Node*> root{nullptr};

public:
    BSTOptimisticCAS() = default;

    bool add(int key) {
        while (true) {
            Node* n = new Node(key);
            Node* expected = nullptr;
            if (root.compare_exchange_strong(expected, n)) {
                return true;
            }
            delete n;
            
            Node* c = root.load();
            while (true) {
                if (c->k == key) {
                    bool expected_m = true;
                    if (c->m.compare_exchange_strong(expected_m, false)) {
                        return true;
                    }
                    return false;
                }
                std::atomic<Node*>* next = (key < c->k) ? &c->l : &c->r;
                Node* t = next->load();
                if (t == nullptr) {
                    Node* n_inner = new Node(key);
                    Node* exp_inner = nullptr;
                    if (next->compare_exchange_strong(exp_inner, n_inner)) {
                        return true;
                    }
                    delete n_inner;
                    t = next->load();
                }
                c = t;
            }
        }
    }

    bool remove(int key) {
        Node* c = root.load();
        while (c != nullptr) {
            if (c->k == key) {
                bool expected_m = false;
                if (c->m.compare_exchange_strong(expected_m, true)) {
                    return true;
                }
                return false;
            }
            c = (key < c->k) ? c->l.load() : c->r.load();
        }
        return false;
    }

    bool contains(int key) {
        Node* c = root.load();
        while (c != nullptr) {
            if (c->k == key) {
                return !c->m.load();
            }
            c = (key < c->k) ? c->l.load() : c->r.load();
        }
        return false;
    }
};
