#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root_(new Node(INT_MIN)) {}

    ~ConcurrentDataStructure() {
        destroyTree(root_);
    }

    bool contains(int key) override {
        Node* node = root_;
        while (true) {
            Node* left = get_unmarked_ref(node->left);
            Node* right = get_unmarked_ref(node->right);
            if (left != nullptr && left->val == key && !is_marked_ref(left)) {
                return true;
            }
            if (right != nullptr && right->val == key && !is_marked_ref(right)) {
                return true;
            }
            if (key < node->val) {
                if (left == nullptr) {
                    return false;
                }
                node = left;
            } else {
                if (right == nullptr) {
                    return false;
                }
                node = right;
            }
        }
    }

    bool add(int key) override {
        Node* node = root_;
        while (true) {
            Node* left = get_unmarked_ref(node->left);
            Node* right = get_unmarked_ref(node->right);
            if (left != nullptr && left->val == key && !is_marked_ref(left)) {
                return false;
            }
            if (right != nullptr && right->val == key && !is_marked_ref(right)) {
                return false;
            }
            if (key < node->val) {
                if (left == nullptr) {
                    Node* new_node = new Node(key);
                    if (node->left.compare_exchange_strong(left, new_node, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    node = left;
                }
            } else {
                if (right == nullptr) {
                    Node* new_node = new Node(key);
                    if (node->right.compare_exchange_strong(right, new_node, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    node = right;
                }
            }
        }
    }

    bool remove(int key) override {
        Node* node = root_;
        while (true) {
            Node* left = get_unmarked_ref(node->left);
            Node* right = get_unmarked_ref(node->right);
            if (left != nullptr && left->val == key && !is_marked_ref(left)) {
                Node* new_left = get_marked_ref(left);
                if (node->left.compare_exchange_strong(left, new_left, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (left->left == nullptr) {
                        if (node->left.compare_exchange_strong(new_left, left->right, std::memory_order_acq_rel)) {
                            delete left;
                            return true;
                        }
                    } else if (left->right == nullptr) {
                        if (node->left.compare_exchange_strong(new_left, left->left, std::memory_order_acq_rel)) {
                            delete left;
                            return true;
                        }
                    } else {
                        Node* successor = minValueNode(left->right);
                        Node* new_node = new Node(successor->val);
                        if (node->left.compare_exchange_strong(new_left, new_node, std::memory_order_acq_rel)) {
                            delete left;
                            return true;
                        }
                    }
                }
            } else if (right != nullptr && right->val == key && !is_marked_ref(right)) {
                Node* new_right = get_marked_ref(right);
                if (node->right.compare_exchange_strong(right, new_right, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (right->left == nullptr) {
                        if (node->right.compare_exchange_strong(new_right, right->right, std::memory_order_acq_rel)) {
                            delete right;
                            return true;
                        }
                    } else if (right->right == nullptr) {
                        if (node->right.compare_exchange_strong(new_right, right->left, std::memory_order_acq_rel)) {
                            delete right;
                            return true;
                        }
                    } else {
                        Node* successor = minValueNode(right->right);
                        Node* new_node = new Node(successor->val);
                        if (node->right.compare_exchange_strong(new_right, new_node, std::memory_order_acq_rel)) {
                            delete right;
                            return true;
                        }
                    }
                }
            } else {
                if (key < node->val) {
                    node = left;
                } else {
                    node = right;
                }
                if (node == nullptr) {
                    return false;
                }
            }
        }
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    Node* root_;

    static Node* get_unmarked_ref(Node* node) {
        uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(node);
        return reinterpret_cast<Node*>(raw_ptr & ~(1ULL));
    }

    static Node* get_marked_ref(Node* node) {
        uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(node);
        return reinterpret_cast<Node*>(raw_ptr | 1ULL);
    }

    static bool is_marked_ref(Node* node) {
        uintptr_t raw_ptr = reinterpret_cast<uintptr_t>(node);
        return raw_ptr & 1ULL;
    }

    Node* minValueNode(Node* node) {
        Node* current = node;
        while (current->left != nullptr) {
            current = current->left;
        }
        return current;
    }

    void destroyTree(Node* node) {
        if (node == nullptr) {
            return;
        }
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }
};