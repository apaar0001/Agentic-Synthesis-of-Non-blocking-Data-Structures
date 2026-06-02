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

    ConcurrentDataStructure() : root_(new Node(INT_MIN)) {}

    ~ConcurrentDataStructure() {
        destroyTree(root_);
    }

    bool contains(int key) {
        Node* node = root_;
        while (true) {
            if (node->val == key) {
                if (is_marked_ref(node->left.load(std::memory_order_acquire))) {
                    continue;
                }
                return true;
            }
            if (key < node->val) {
                if (node->left.load(std::memory_order_acquire) == nullptr) {
                    return false;
                }
                node = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            } else {
                if (node->right.load(std::memory_order_acquire) == nullptr) {
                    return false;
                }
                node = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            }
        }
    }

    bool add(int key) {
        Node* node = root_;
        while (true) {
            if (node->val == key) {
                if (is_marked_ref(node->left.load(std::memory_order_acquire))) {
                    continue;
                }
                return false;
            }
            if (key < node->val) {
                if (node->left.load(std::memory_order_acquire) == nullptr) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (node->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        return true;
                    }
                    delete newNode;
                    continue;
                }
                node = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            } else {
                if (node->right.load(std::memory_order_acquire) == nullptr) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (node->right.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        return true;
                    }
                    delete newNode;
                    continue;
                }
                node = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            }
        }
    }

    bool remove(int key) {
        Node* node = root_;
        while (true) {
            if (node->val == key) {
                if (is_marked_ref(node->left.load(std::memory_order_acquire))) {
                    continue;
                }
                Node* left = node->left.load(std::memory_order_acquire);
                Node* right = node->right.load(std::memory_order_acquire);
                if (left == nullptr) {
                    Node* expected = right;
                    if (node->right.compare_exchange_strong(expected, get_marked_ref(right), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        // Node has been marked
                        return true;
                    }
                    continue;
                } else if (right == nullptr) {
                    Node* expected = left;
                    if (node->left.compare_exchange_strong(expected, get_marked_ref(left), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        // Node has been marked
                        return true;
                    }
                    continue;
                } else {
                    Node* successor = minValueNode(right);
                    Node* successorLeft = successor->left.load(std::memory_order_acquire);
                    Node* successorRight = successor->right.load(std::memory_order_acquire);
                    if (successorLeft == nullptr) {
                        Node* expected = successorRight;
                        if (successor->right.compare_exchange_strong(expected, get_marked_ref(successorRight), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            // Node has been marked
                            node->val = successor->val;
                            return true;
                        }
                        continue;
                    } else {
                        Node* expected = successorLeft;
                        if (successor->left.compare_exchange_strong(expected, get_marked_ref(successorLeft), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            // Node has been marked
                            node->val = successor->val;
                            return true;
                        }
                        continue;
                    }
                }
            }
            if (key < node->val) {
                if (node->left.load(std::memory_order_acquire) == nullptr) {
                    return false;
                }
                node = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            } else {
                if (node->right.load(std::memory_order_acquire) == nullptr) {
                    return false;
                }
                node = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            }
        }
    }

private:
    Node* root_;

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    Node* minValueNode(Node* node) {
        Node* current = node;
        while (current->left.load(std::memory_order_acquire) != nullptr) {
            current = get_unmarked_ref(current->left.load(std::memory_order_acquire));
        }
        return current;
    }

    void destroyTree(Node* node) {
        if (node == nullptr) {
            return;
        }
        destroyTree(node->left.load(std::memory_order_relaxed));
        destroyTree(node->right.load(std::memory_order_relaxed));
        delete node;
    }
};