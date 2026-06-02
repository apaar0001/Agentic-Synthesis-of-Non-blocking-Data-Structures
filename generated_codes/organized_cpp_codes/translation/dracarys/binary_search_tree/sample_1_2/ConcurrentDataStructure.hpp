#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root_(new Node(INT_MIN)) {}

    ~ConcurrentDataStructure() override {
        destroyTree(root_.load(std::memory_order_acquire));
    }

    bool contains(int key) override {
        return containsHelper(root_.load(std::memory_order_acquire), key);
    }

    bool add(int key) override {
        if (contains(key)) {
            return false;
        }
        return addHelper(root_.load(std::memory_order_acquire), key);
    }

    bool remove(int key) override {
        if (!contains(key)) {
            return false;
        }
        return removeHelper(root_.load(std::memory_order_acquire), key);
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    std::atomic<Node*> root_;

    bool containsHelper(Node* node, int key) {
        while (node != nullptr) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
                continue;
            }
            if (key < node->val) {
                node = node->left.load(std::memory_order_acquire);
            } else if (key > node->val) {
                node = node->right.load(std::memory_order_acquire);
            } else {
                return true;
            }
        }
        return false;
    }

    bool addHelper(Node* node, int key) {
        while (true) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
                continue;
            }
            if (key < node->val) {
                Node* left = node->left.load(std::memory_order_acquire);
                if (left == nullptr) {
                    Node* newNode = new Node(key);
                    if (node->left.compare_exchange_strong(left, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    node = left;
                }
            } else if (key > node->val) {
                Node* right = node->right.load(std::memory_order_acquire);
                if (right == nullptr) {
                    Node* newNode = new Node(key);
                    if (node->right.compare_exchange_strong(right, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    node = right;
                }
            } else {
                return false;
            }
        }
    }

    bool removeHelper(Node* node, int key) {
        while (true) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
                continue;
            }
            if (key < node->val) {
                Node* left = node->left.load(std::memory_order_acquire);
                if (left != nullptr && left->val == key) {
                    Node* markedLeft = get_marked_ref(left);
                    if (node->left.compare_exchange_strong(left, markedLeft, std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (left->left.load(std::memory_order_acquire) == nullptr && left->right.load(std::memory_order_acquire) == nullptr) {
                            node->left.compare_exchange_strong(markedLeft, nullptr, std::memory_order_acq_rel);
                        }
                        return true;
                    }
                } else {
                    node = left;
                }
            } else if (key > node->val) {
                Node* right = node->right.load(std::memory_order_acquire);
                if (right != nullptr && right->val == key) {
                    Node* markedRight = get_marked_ref(right);
                    if (node->right.compare_exchange_strong(right, markedRight, std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (right->left.load(std::memory_order_acquire) == nullptr && right->right.load(std::memory_order_acquire) == nullptr) {
                            node->right.compare_exchange_strong(markedRight, nullptr, std::memory_order_acq_rel);
                        }
                        return true;
                    }
                } else {
                    node = right;
                }
            } else {
                Node* markedNode = get_marked_ref(node);
                if (node->left.compare_exchange_strong(node, markedNode, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (node->left.load(std::memory_order_acquire) == nullptr && node->right.load(std::memory_order_acquire) == nullptr) {
                        root_.compare_exchange_strong(markedNode, nullptr, std::memory_order_acq_rel);
                    }
                    return true;
                }
            }
        }
    }

    void destroyTree(Node* node) {
        if (node == nullptr) {
            return;
        }
        destroyTree(node->left.load(std::memory_order_acquire));
        destroyTree(node->right.load(std::memory_order_acquire));
        delete node;
    }
};