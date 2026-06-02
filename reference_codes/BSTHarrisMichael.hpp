#pragma once
#include <atomic>
#include <limits>

class BSTHarrisMichael {
private:
    struct Node {
        const int key;
        std::atomic<Node*> left{nullptr};
        std::atomic<Node*> right{nullptr};
        std::atomic<bool> marked{false};

        Node(int k) : key(k) {}
    };

    Node* root;

public:
    BSTHarrisMichael() {
        root = new Node(std::numeric_limits<int>::min());
    }

    bool add(int key) {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* childRef = nullptr;
            Node* curr = root;

            while (curr != nullptr) {
                if (key == curr->key) {
                    if (!curr->marked.load()) {
                        return false; 
                    }
                    bool expected = true;
                    if (curr->marked.compare_exchange_strong(expected, false)) {
                        return true;
                    }
                    return false; 
                }
                parent = curr;
                if (key < curr->key) {
                    childRef = &curr->left;
                    curr = curr->left.load();
                } else {
                    childRef = &curr->right;
                    curr = curr->right.load();
                }
            }

            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (childRef->compare_exchange_strong(expected, newNode)) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) {
        Node* curr = root;
        while (curr != nullptr) {
            if (key == curr->key) {
                bool expected = false;
                if (curr->marked.compare_exchange_strong(expected, true)) {
                    return true;
                }
                return false;
            }
            if (key < curr->key) {
                curr = curr->left.load();
            } else {
                curr = curr->right.load();
            }
        }
        return false;
    }

    bool contains(int key) {
        Node* curr = root;
        while (curr != nullptr) {
            if (key == curr->key) {
                return !curr->marked.load();
            }
            if (key < curr->key) {
                curr = curr->left.load();
            } else {
                curr = curr->right.load();
            }
        }
        return false;
    }
};
