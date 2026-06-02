#pragma once
#include <atomic>
#include <limits>

class BSTSentinelNode {
private:
    struct Node {
        const int key;
        std::atomic<bool> marked{false}; 
        std::atomic<Node*> left{nullptr};
        std::atomic<Node*> right{nullptr};

        Node(int k) : key(k) {}
    };

    std::atomic<Node*> root{nullptr};

public:
    BSTSentinelNode() = default;

    bool add(int key) {
        while (true) {
            Node* n = new Node(key);
            Node* r = root.load();
            if (r == nullptr) {
                Node* expected = nullptr;
                if (root.compare_exchange_strong(expected, n)) {
                    return true;
                }
                delete n;
                continue;
            }
            delete n; // we recreate it inside if needed
            Node* c = r;
            while (true) {
                if (c->key == key) {
                    bool expected_marked = true;
                    if (c->marked.compare_exchange_strong(expected_marked, false)) {
                        return true;
                    }
                    return false;
                }
                std::atomic<Node*>* next = (key < c->key) ? &c->left : &c->right;
                Node* m = next->load();
                if (m == nullptr) {
                    Node* n_inner = new Node(key);
                    Node* exp_inner = nullptr;
                    if (next->compare_exchange_strong(exp_inner, n_inner)) {
                        return true;
                    }
                    delete n_inner;
                    m = next->load();
                    if (m == nullptr) {
                        continue;
                    }
                }
                c = m;
            }
        }
    }

    bool remove(int key) {
        Node* c = root.load();
        while (c != nullptr) {
            if (c->key == key) {
                bool expected_marked = false;
                if (c->marked.compare_exchange_strong(expected_marked, true)) {
                    return true;
                }
                return false;
            }
            c = (key < c->key) ? c->left.load() : c->right.load();
        }
        return false;
    }

    bool contains(int key) {
        Node* c = root.load();
        while (c != nullptr) {
            if (c->key == key) {
                return !c->marked.load();
            }
            c = (key < c->key) ? c->left.load() : c->right.load();
        }
        return false;
    }
};
