#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>

/**
 * Lock-free Binary Search Tree with CAS-based insertion
 * and CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in structure.
 * This keeps deletion simple and avoids unsafe memory reclamation issues.
 *
 * Linearization:
 *   add():      CAS on parent child pointer OR CAS marked true->false
 *   remove():   CAS on node->marked false->true
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

    Node* root;

    void deleteSubtree(Node* node) {
        if (!node) return;
        Node* l = node->left.load(std::memory_order_relaxed);
        Node* r = node->right.load(std::memory_order_relaxed);
        deleteSubtree(l);
        deleteSubtree(r);
        delete node;
    }

    Node* findNode(int key) const {
        Node* curr = root->right.load(std::memory_order_acquire);

        while (curr) {
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

public:
    LockFreeBST() {
        root = new Node(INT_MIN);
    }

    ~LockFreeBST() override {
        deleteSubtree(root);
    }

    bool contains(int key) override {
        Node* node = findNode(key);
        return node && !node->marked.load(std::memory_order_acquire);
    }

    bool add(int key) override {
        while (true) {
            Node* parent = root;
            Node* curr = root->right.load(std::memory_order_acquire);
            bool goLeft = false;

            while (curr) {
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

                    continue;
                }

                parent = curr;

                if (key < curr->key) {
                    goLeft = true;
                    curr = curr->left.load(std::memory_order_acquire);
                } else {
                    goLeft = false;
                    curr = curr->right.load(std::memory_order_acquire);
                }
            }

            Node* newNode = new Node(key);
            std::atomic<Node*>& child =
                (parent == root || !goLeft) ? parent->right : parent->left;

            Node* expected = nullptr;
            if (child.compare_exchange_strong(
                    expected, newNode,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete newNode;
        }
    }

    bool remove(int key) override {
        Node* node = findNode(key);
        if (!node) return false;

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