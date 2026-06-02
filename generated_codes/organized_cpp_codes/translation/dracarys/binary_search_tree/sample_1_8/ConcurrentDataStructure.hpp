#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    ConcurrentDataStructure() : root_(nullptr) {}

    bool contains(int key) {
        Node* node = root_.load(std::memory_order_acquire);
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

    bool add(int key) {
        while (true) {
            Node* node = root_.load(std::memory_order_acquire);
            while (node != nullptr) {
                if (is_marked_ref(node)) {
                    node = get_unmarked_ref(node);
                    continue;
                }
                if (key < node->val) {
                    Node* left = node->left.load(std::memory_order_acquire);
                    if (left == nullptr) {
                        Node* new_node = new Node(key);
                        if (node->left.compare_exchange_strong(left, new_node, std::memory_order_acq_rel)) {
                            return true;
                        }
                    }
                    node = node->left.load(std::memory_order_acquire);
                } else if (key > node->val) {
                    Node* right = node->right.load(std::memory_order_acquire);
                    if (right == nullptr) {
                        Node* new_node = new Node(key);
                        if (node->right.compare_exchange_strong(right, new_node, std::memory_order_acq_rel)) {
                            return true;
                        }
                    }
                    node = node->right.load(std::memory_order_acquire);
                } else {
                    return false;
                }
            }
            Node* new_node = new Node(key);
            Node* expected = nullptr;
            if (root_.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) {
        while (true) {
            Node* node = root_.load(std::memory_order_acquire);
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
                    Node* left = node->left.load(std::memory_order_acquire);
                    Node* right = node->right.load(std::memory_order_acquire);
                    if (left == nullptr) {
                        if (node->right.compare_exchange_strong(right, get_marked_ref(right), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                    } else if (right == nullptr) {
                        if (node->left.compare_exchange_strong(left, get_marked_ref(left), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                    } else {
                        Node* min_node = minValueNode(right);
                        Node* min_left = min_node->left.load(std::memory_order_acquire);
                        if (min_left == nullptr) {
                            if (min_node->left.compare_exchange_strong(min_left, get_marked_ref(min_left), std::memory_order_acq_rel)) {
                                // Node has been marked
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }
    }

private:
    std::atomic<Node*> root_;

    static Node* minValueNode(Node* node) {
        while (node->left.load(std::memory_order_acquire) != nullptr) {
            node = node->left.load(std::memory_order_acquire);
        }
        return node;
    }

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }
};