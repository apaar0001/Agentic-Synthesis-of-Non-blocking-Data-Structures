#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root(new Node(INT_MIN)) {}

    bool contains(int key) override;
    bool add(int key) override;
    bool remove(int key) override;

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

    Node* root;

    Node* find(Node* node, int key);
    bool add(Node* node, int key);
    bool remove(Node* node, int key);
};

ConcurrentDataStructure::Node* ConcurrentDataStructure::find(Node* node, int key) {
    while (node != nullptr) {
        if (key < node->val) {
            node = get_unmarked_ref(node->left);
        } else if (key > node->val) {
            node = get_unmarked_ref(node->right);
        } else {
            return node;
        }
    }
    return nullptr;
}

bool ConcurrentDataStructure::contains(int key) {
    return find(root, key) != nullptr;
}

bool ConcurrentDataStructure::add(int key) {
    Node* node = find(root, key);
    if (node != nullptr) {
        return false;
    }
    return add(root, key);
}

bool ConcurrentDataStructure::add(Node* node, int key) {
    if (node->val == INT_MIN) {
        Node* newNode = new Node(key);
        Node* expected = nullptr;
        if (node->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
            return true;
        }
    } else {
        Node* child = (key < node->val) ? node->left : node->right;
        Node* unmarkedChild = get_unmarked_ref(child);
        if (unmarkedChild == nullptr) {
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (node->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        } else {
            return add(unmarkedChild, key);
        }
    }
    return false;
}

bool ConcurrentDataStructure::remove(int key) {
    return remove(root, key);
}

bool ConcurrentDataStructure::remove(Node* node, int key) {
    if (node->val == INT_MIN) {
        return false;
    }

    Node* child = (key < node->val) ? node->left : node->right;
    Node* unmarkedChild = get_unmarked_ref(child);
    if (unmarkedChild == nullptr) {
        return false;
    }

    if (key < node->val) {
        if (unmarkedChild->val == key) {
            Node* markedChild = get_marked_ref(unmarkedChild);
            if (node->left.compare_exchange_strong(unmarkedChild, markedChild, std::memory_order_acq_rel)) {
                // Node has been marked
                Node* newNode = unmarkedChild->right;
                if (node->left.compare_exchange_strong(markedChild, newNode, std::memory_order_acq_rel)) {
                    delete unmarkedChild;
                    return true;
                }
            }
        } else {
            return remove(unmarkedChild, key);
        }
    } else {
        if (unmarkedChild->val == key) {
            Node* markedChild = get_marked_ref(unmarkedChild);
            if (node->right.compare_exchange_strong(unmarkedChild, markedChild, std::memory_order_acq_rel)) {
                // Node has been marked
                Node* newNode = unmarkedChild->left;
                if (node->right.compare_exchange_strong(markedChild, newNode, std::memory_order_acq_rel)) {
                    delete unmarkedChild;
                    return true;
                }
            }
        } else {
            return remove(unmarkedChild, key);
        }
    }
    return false;
}