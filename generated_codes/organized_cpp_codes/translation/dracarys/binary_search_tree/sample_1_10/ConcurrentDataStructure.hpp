#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root(new Node(INT_MIN)) {}

    bool contains(int key) override {
        return contains(key, root);
    }

    bool add(int key) override {
        while (true) {
            Node* node = root;
            Node* nextLeft;
            Node* nextRight;
            while (true) {
                nextLeft = node->left.load(std::memory_order_acquire);
                nextRight = node->right.load(std::memory_order_acquire);
                if (nextLeft != nullptr && !is_marked_ref(nextLeft) && nextLeft->val < key) {
                    node = nextLeft;
                } else if (nextRight != nullptr && !is_marked_ref(nextRight) && nextRight->val > key) {
                    node = nextRight;
                } else {
                    break;
                }
            }

            if (node->val == key) {
                return false;
            }

            Node* newNode = new Node(key);
            if (key < node->val) {
                if (node->left.compare_exchange_strong(nextLeft, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                if (node->right.compare_exchange_strong(nextRight, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* node = root;
            Node* nextLeft;
            Node* nextRight;
            while (true) {
                nextLeft = node->left.load(std::memory_order_acquire);
                nextRight = node->right.load(std::memory_order_acquire);
                if (nextLeft != nullptr && !is_marked_ref(nextLeft) && nextLeft->val < key) {
                    node = nextLeft;
                } else if (nextRight != nullptr && !is_marked_ref(nextRight) && nextRight->val > key) {
                    node = nextRight;
                } else {
                    break;
                }
            }

            if (node->val != key) {
                return false;
            }

            Node* markedNode = get_marked_ref(node);
            if (node->left.compare_exchange_strong(node, markedNode, std::memory_order_acq_rel)) {
                // Node has been marked
                if (node->left.load(std::memory_order_acquire) == markedNode) {
                    Node* next = node->right.load(std::memory_order_acquire);
                    if (next != nullptr && !is_marked_ref(next)) {
                        node->right.compare_exchange_strong(next, nullptr, std::memory_order_acq_rel);
                    }
                    node->left.compare_exchange_strong(markedNode, nullptr, std::memory_order_acq_rel);
                }
                return true;
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

    Node* root;

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    bool contains(int key, Node* node) {
        while (true) {
            Node* nextLeft = node->left.load(std::memory_order_acquire);
            Node* nextRight = node->right.load(std::memory_order_acquire);
            if (nextLeft != nullptr && !is_marked_ref(nextLeft) && nextLeft->val < key) {
                node = nextLeft;
            } else if (nextRight != nullptr && !is_marked_ref(nextRight) && nextRight->val > key) {
                node = nextRight;
            } else {
                return node->val == key;
            }
        }
    }
};