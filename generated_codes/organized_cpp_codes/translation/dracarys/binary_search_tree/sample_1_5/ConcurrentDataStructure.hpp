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

    bool add(Node* node, int key);
    bool remove(Node* node, int key);
    Node* findMin(Node* node);
    Node* find(Node* node, int key);
};

bool ConcurrentDataStructure::contains(int key) {
    Node* node = root;
    while (true) {
        if (key < node->val) {
            node = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            if (node == nullptr) {
                return false;
            }
        } else if (key > node->val) {
            node = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            if (node == nullptr) {
                return false;
            }
        } else {
            return !is_marked_ref(node);
        }
    }
}

bool ConcurrentDataStructure::add(int key) {
    return add(root, key);
}

bool ConcurrentDataStructure::remove(int key) {
    return remove(root, key);
}

bool ConcurrentDataStructure::add(Node* node, int key) {
    if (key < node->val) {
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        if (left == nullptr) {
            Node* new_node = new Node(key);
            if (node->left.compare_exchange_strong(left, new_node, std::memory_order_acq_rel)) {
                return true;
            }
        } else if (is_marked_ref(left)) {
            Node* new_left = find(node, key);
            if (new_left->val == key) {
                return false;
            }
            return add(new_left, key);
        } else {
            return add(left, key);
        }
    } else if (key > node->val) {
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        if (right == nullptr) {
            Node* new_node = new Node(key);
            if (node->right.compare_exchange_strong(right, new_node, std::memory_order_acq_rel)) {
                return true;
            }
        } else if (is_marked_ref(right)) {
            Node* new_right = find(node, key);
            if (new_right->val == key) {
                return false;
            }
            return add(new_right, key);
        } else {
            return add(right, key);
        }
    } else {
        return !is_marked_ref(node);
    }
    return add(node, key); // retry
}

bool ConcurrentDataStructure::remove(Node* node, int key) {
    if (key < node->val) {
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        if (left == nullptr) {
            return false;
        } else if (is_marked_ref(left)) {
            return remove(node, key); // retry
        } else {
            return remove(left, key);
        }
    } else if (key > node->val) {
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        if (right == nullptr) {
            return false;
        } else if (is_marked_ref(right)) {
            return remove(node, key); // retry
        } else {
            return remove(right, key);
        }
    } else {
        if (is_marked_ref(node)) {
            return false;
        }
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        if (left == nullptr) {
            Node* new_right = get_marked_ref(right);
            if (node->right.compare_exchange_strong(right, new_right, std::memory_order_acq_rel)) {
                // Node has been marked
                return true;
            }
        } else if (right == nullptr) {
            Node* new_left = get_marked_ref(left);
            if (node->left.compare_exchange_strong(left, new_left, std::memory_order_acq_rel)) {
                // Node has been marked
                return true;
            }
        } else {
            Node* min_node = findMin(right);
            node->val = min_node->val;
            return remove(right, min_node->val);
        }
    }
    return remove(node, key); // retry
}

ConcurrentDataStructure::Node* ConcurrentDataStructure::findMin(Node* node) {
    while (true) {
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        if (left == nullptr) {
            return node;
        }
        node = left;
    }
}

ConcurrentDataStructure::Node* ConcurrentDataStructure::find(Node* node, int key) {
    while (true) {
        if (key < node->val) {
            Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            if (left == nullptr) {
                return node;
            }
            node = left;
        } else if (key > node->val) {
            Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            if (right == nullptr) {
                return node;
            }
            node = right;
        } else {
            return node;
        }
    }
}