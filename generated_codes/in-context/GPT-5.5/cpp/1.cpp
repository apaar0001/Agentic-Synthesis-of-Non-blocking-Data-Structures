#pragma once

#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>

/**
 * Lock-free Binary Search Tree with CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in structure.
 * contains() treats marked nodes as absent. add() can revive a marked node
 * with the same key using CAS on marked.
 *
 * Linearization:
 *   add():     CAS on parent child pointer OR CAS marked true->false
 *   remove():  CAS on node->marked false->true
 *   contains(): read of node->marked
 */
class LockFreeBST : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<bool> marked;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int k) : key(k), marked(false), left(nullptr), right(nullptr) {}
    };

    std::atomic<Node*> root;

    Node* findNode(int key) const {
        Node* curr = root.load(std::memory_order_acquire);

        while (curr != nullptr) {
            if (key == curr->key) {
                return curr;
            } else if (key < curr->key) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }

        return nullptr;
    }

    void deleteSubtree(Node* node) {
        if (node == nullptr) return;

        Node* leftChild = node->left.load(std::memory_order_relaxed);
        Node* rightChild = node->right.load(std::memory_order_relaxed);

        deleteSubtree(leftChild);
        deleteSubtree(rightChild);
        delete node;
    }

public:
    LockFreeBST() : root(nullptr) {}

    ~LockFreeBST() override {
        deleteSubtree(root.load(std::memory_order_relaxed));
    }

    bool contains(int key) override {
        Node* node = findNode(key);
        if (node == nullptr) return false;

        return !node->marked.load(std::memory_order_acquire);
    }

    bool add(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);

            if (curr == nullptr) {
                Node* newNode = new Node(key);
                Node* expected = nullptr;

                if (root.compare_exchange_strong(
                        expected, newNode,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return true;
                }

                delete newNode;
                continue;
            }

            Node* parent = nullptr;

            while (curr != nullptr) {
                parent = curr;

                if (key == curr->key) {
                    bool expectedMarked = true;
                    if (curr->marked.compare_exchange_strong(
                            expectedMarked, false,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        return true;
                    }

                    return false;
                } else if (key < curr->key) {
                    curr = curr->left.load(std::memory_order_acquire);
                } else {
                    curr = curr->right.load(std::memory_order_acquire);
                }
            }

            Node* newNode = new Node(key);
            std::atomic<Node*>& childRef = (key < parent->key) ? parent->left : parent->right;
            Node* expected = nullptr;

            if (childRef.compare_exchange_strong(
                    expected, newNode,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete newNode;
        }
    }

    bool remove(int key) override {
        Node* victim = findNode(key);

        if (victim == nullptr) return false;

        bool expected = false;
        if (!victim->marked.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return false;
        }

        // Node has been marked
        return true;
    }
};