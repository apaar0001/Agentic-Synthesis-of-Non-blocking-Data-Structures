#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/**
 * Lock-free Binary Search Tree with CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in the tree.
 * This keeps removal simple and avoids races from concurrent child rewiring.
 * A later add() of the same key can unmark the logically deleted node.
 *
 * Linearization:
 *   add():      CAS on parent->left/right = newNode, or CAS marked true→false
 *   remove():   CAS on node->marked = true
 *   contains(): read of node->marked
 */
class LockFreeBST : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        std::atomic<bool> marked;

        Node(int k) : key(k), left(nullptr), right(nullptr), marked(false) {}
    };

    std::atomic<Node*> root;

    static void destroy(Node* node) {
        if (!node) return;
        Node* l = node->left.load(std::memory_order_relaxed);
        Node* r = node->right.load(std::memory_order_relaxed);
        destroy(l);
        destroy(r);
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
            }
            if (key < curr->key) {
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
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
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
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    return false;
                }

                std::atomic<Node*>& child = (key < curr->key) ? curr->left : curr->right;
                Node* next = child.load(std::memory_order_acquire);

                if (!next) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (child.compare_exchange_strong(
                            expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    delete newNode;
                    break;
                }

                curr = next;
            }
        }
    }

    bool remove(int key) override {
        Node* curr = root.load(std::memory_order_acquire);

        while (curr) {
            if (key == curr->key) {
                bool expected = false;
                if (!curr->marked.compare_exchange_strong(
                        expected, true,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return false;
                }
                // Node has been marked
                return true;
            }

            if (key < curr->key) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
};