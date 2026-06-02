#pragma once

#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/**
 * Lock-free Binary Search Tree with CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in structure.
 * contains() treats marked nodes as absent. add() can revive a marked node
 * with the same key by CAS on marked true -> false.
 *
 * Linearization:
 *   add():     CAS on child pointer or CAS marked true -> false
 *   remove():  CAS on node->marked false -> true
 *   contains(): read of node->marked
 */
class LockFreeBST : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<bool> marked;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int k)
            : key(k), marked(false), left(nullptr), right(nullptr) {}
    };

    std::atomic<Node*> root;

    void destroy(Node* node) {
        if (!node) return;
        destroy(node->left.load(std::memory_order_relaxed));
        destroy(node->right.load(std::memory_order_relaxed));
        delete node;
    }

public:
    LockFreeBST() : root(nullptr) {}

    ~LockFreeBST() override {
        destroy(root.load(std::memory_order_relaxed));
    }

    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);

        while (curr) {
            if (key == curr->key) {
                return !curr->marked.load(std::memory_order_acquire);
            } else if (key < curr->key) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }

        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);

            if (!curr) {
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

            while (true) {
                if (key == curr->key) {
                    bool expected = true;

                    if (curr->marked.compare_exchange_strong(
                            expected, false,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        return true;
                    }

                    if (!curr->marked.load(std::memory_order_acquire)) {
                        return false;
                    }

                    break;
                }

                std::atomic<Node*>* childRef =
                    (key < curr->key) ? &curr->left : &curr->right;

                Node* child = childRef->load(std::memory_order_acquire);

                if (!child) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;

                    if (childRef->compare_exchange_strong(
                            expected, newNode,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        return true;
                    }

                    delete newNode;
                    break;
                }

                curr = child;
            }
        }
    }

    bool remove(int key) override {
        Node* curr = root.load(std::memory_order_acquire);

        while (curr) {
            if (key == curr->key) {
                bool expected = false;

                if (curr->marked.compare_exchange_strong(
                        expected, true,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    // Node has been marked
                    return true;
                }

                return false;
            } else if (key < curr->key) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }

        return false;
    }
};