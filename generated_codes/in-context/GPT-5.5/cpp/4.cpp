#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/**
 * Lock-free Binary Search Tree with CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in structure.
 * Traversal ignores marked nodes for contains().
 *
 * Linearization:
 *   add():     CAS on parent->left/right = newNode, or CAS marked true→false
 *   remove():  CAS on node->marked = true
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

        Node* l = node->left.load(std::memory_order_relaxed);
        Node* r = node->right.load(std::memory_order_relaxed);

        deleteSubtree(l);
        deleteSubtree(r);

        delete node;
    }

public:
    LockFreeBST() : root(nullptr) {}

    ~LockFreeBST() override {
        Node* r = root.load(std::memory_order_relaxed);
        deleteSubtree(r);
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

                if (key < curr->key) {
                    Node* leftChild = curr->left.load(std::memory_order_acquire);

                    if (leftChild == nullptr) {
                        Node* newNode = new Node(key);
                        Node* expected = nullptr;

                        if (curr->left.compare_exchange_strong(
                                expected, newNode,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire)) {
                            return true;
                        }

                        delete newNode;
                        break;
                    }

                    curr = leftChild;
                } else {
                    Node* rightChild = curr->right.load(std::memory_order_acquire);

                    if (rightChild == nullptr) {
                        Node* newNode = new Node(key);
                        Node* expected = nullptr;

                        if (curr->right.compare_exchange_strong(
                                expected, newNode,
                                std::memory_order_acq_rel,
                                std::memory_order_acquire)) {
                            return true;
                        }

                        delete newNode;
                        break;
                    }

                    curr = rightChild;
                }
            }
        }
    }

    bool remove(int key) override {
        Node* node = findNode(key);
        if (node == nullptr) return false;

        bool expected = false;

        if (!node->marked.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel,
                std::memory_order_acquire)) {
            return false;
        }

        // Node has been marked
        return true;
    }
};