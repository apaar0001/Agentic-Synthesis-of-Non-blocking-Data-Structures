#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>

/**
 * Lock-free BST using mark-based logical deletion.
 *
 * - Internal nodes store keys; no physical removal.
 * - add(): if key found & marked → CAS unmark (re-insert); else CAS new node.
 * - remove(): CAS mark on target node.
 * - contains(): traverse, check unmarked target.
 * - Lock-freedom: every path uses CAS retry loops.
 */
class LockFreeBSTree : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        std::atomic<bool> marked;  // true = logically deleted

        Node(int k) : key(k), left(nullptr), right(nullptr), marked(false) {}
    };

    Node* root;  // sentinel with key = INT_MIN, never marked

public:
    LockFreeBSTree() {
        root = new Node(INT_MIN);
    }

    ~LockFreeBSTree() override {
        destroyTree(root);
    }

    bool add(int key) override {
        if (key == INT_MIN) return false;
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* childRef = nullptr;
            Node* curr = root;

            // Traverse to find key or insertion point
            while (curr != nullptr) {
                if (key == curr->key) {
                    // Key exists in tree structure
                    if (!curr->marked.load(std::memory_order_acquire)) {
                        return false;  // alive → duplicate
                    }
                    // Marked (deleted) → try to resurrect via CAS
                    bool expected = true;
                    if (curr->marked.compare_exchange_strong(expected, false,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;  // successfully re-added
                    }
                    // Another thread unmarked it first → it's alive → duplicate
                    return false;
                }
                parent = curr;
                if (key < curr->key) {
                    childRef = &curr->left;
                    curr = curr->left.load(std::memory_order_acquire);
                } else {
                    childRef = &curr->right;
                    curr = curr->right.load(std::memory_order_acquire);
                }
            }

            // curr == nullptr: insert new node
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (childRef->compare_exchange_strong(expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete newNode;
            // CAS failed — another thread inserted here, retry from root
        }
    }

    bool remove(int key) override {
        if (key == INT_MIN) return false;
        Node* curr = root;
        while (curr != nullptr) {
            if (key == curr->key) {
                bool expected = false;
                // Linearization: CAS marked false→true
                if (curr->marked.compare_exchange_strong(expected, true,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    return true;
                }
                return false;  // already marked by another thread
            }
            if (key < curr->key) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }

    bool contains(int key) override {
        if (key == INT_MIN) return false;
        Node* curr = root;
        while (curr != nullptr) {
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

private:
    void destroyTree(Node* node) {
        if (!node) return;
        Node* left = node->left.load(std::memory_order_relaxed);
        Node* right = node->right.load(std::memory_order_relaxed);
        delete node;
        destroyTree(left);
        destroyTree(right);
    }
};
