#pragma once
#include <atomic>
#include <limits>

class BSTLazyDeletion {
private:
    struct Node {
        const int key;
        std::atomic<bool> isDeleted{false};
        std::atomic<Node*> left{nullptr};
        std::atomic<Node*> right{nullptr};

        Node(int k) : key(k) {}
    };

    std::atomic<Node*> root{nullptr};

public:
    BSTLazyDeletion() = default;

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
                if (c->key == key) {
                    bool expected_del = true;
                    if (c->isDeleted.compare_exchange_strong(expected_del, false)) {
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
                } else {
                    c = m;
                }
            }
        }
    }

    bool remove(int key) {
        Node* c = root.load();
        while (c != nullptr) {
            if (c->key == key) {
                bool expected_del = false;
                if (c->isDeleted.compare_exchange_strong(expected_del, true)) {
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
                return !c->isDeleted.load();
            }
            c = (key < c->key) ? c->left.load() : c->right.load();
        }
        return false;
    }
};
