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

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1;
    }

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1);
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }

    std::atomic<Node*> root;

    bool add(Node* node, int key);
    bool remove(Node* node, int key);
    Node* findNode(Node* node, int key);
    Node* findMinNode(Node* node);
};

bool ConcurrentDataStructure::contains(int key) {
    Node* node = root.load(std::memory_order_acquire);
    while (node) {
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

bool ConcurrentDataStructure::add(int key) {
    return add(root.load(std::memory_order_acquire), key);
}

bool ConcurrentDataStructure::remove(int key) {
    return remove(root.load(std::memory_order_acquire), key);
}

bool ConcurrentDataStructure::add(Node* node, int key) {
    if (node->val == key) {
        return false;
    }

    if (key < node->val) {
        Node* left = node->left.load(std::memory_order_acquire);
        if (is_marked_ref(left)) {
            left = get_unmarked_ref(left);
            if (!node->left.compare_exchange_strong(left, left, std::memory_order_acq_rel)) {
                return add(node, key);
            }
        }

        if (!left) {
            Node* newNode = new Node(key);
            Node* nullNode = nullptr;
            if (node->left.compare_exchange_strong(nullNode, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
            return add(node, key);
        } else {
            return add(left, key);
        }
    } else {
        Node* right = node->right.load(std::memory_order_acquire);
        if (is_marked_ref(right)) {
            right = get_unmarked_ref(right);
            if (!node->right.compare_exchange_strong(right, right, std::memory_order_acq_rel)) {
                return add(node, key);
            }
        }

        if (!right) {
            Node* newNode = new Node(key);
            Node* nullNode = nullptr;
            if (node->right.compare_exchange_strong(nullNode, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
            return add(node, key);
        } else {
            return add(right, key);
        }
    }
}

bool ConcurrentDataStructure::remove(Node* node, int key) {
    if (node->val == key) {
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);

        if (is_marked_ref(left) || is_marked_ref(right)) {
            return false;
        }

        if (!left && !right) {
            Node* nullNode = nullptr;
            if (node->left.compare_exchange_strong(nullNode, get_marked_ref(nullNode), std::memory_order_acq_rel) &&
                node->right.compare_exchange_strong(nullNode, get_marked_ref(nullNode), std::memory_order_acq_rel)) {
                // Node has been marked
                return true;
            }
        } else if (!left) {
            Node* nullNode = nullptr;
            if (node->left.compare_exchange_strong(nullNode, get_marked_ref(nullNode), std::memory_order_acq_rel) &&
                node->right.compare_exchange_strong(right, get_marked_ref(right), std::memory_order_acq_rel)) {
                // Node has been marked
                return true;
            }
        } else if (!right) {
            Node* nullNode = nullptr;
            if (node->left.compare_exchange_strong(left, get_marked_ref(left), std::memory_order_acq_rel) &&
                node->right.compare_exchange_strong(nullNode, get_marked_ref(nullNode), std::memory_order_acq_rel)) {
                // Node has been marked
                return true;
            }
        } else {
            Node* minNode = findMinNode(right);
            if (minNode->val == key) {
                return false;
            }

            Node* minNodeParent = findNode(node, minNode->val);
            if (minNodeParent->right.load(std::memory_order_acquire) != minNode) {
                return remove(node, key);
            }

            if (minNode->left.load(std::memory_order_acquire) == nullptr) {
                if (minNodeParent->right.compare_exchange_strong(minNode, minNode->right.load(std::memory_order_acquire), std::memory_order_acq_rel)) {
                    // Node has been marked
                    return true;
                }
            } else {
                Node* nullNode = nullptr;
                if (minNode->left.compare_exchange_strong(nullNode, get_marked_ref(nullNode), std::memory_order_acq_rel) &&
                    minNode->right.compare_exchange_strong(nullptr, get_marked_ref(nullptr), std::memory_order_acq_rel)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }

    if (key < node->val) {
        Node* left = node->left.load(std::memory_order_acquire);
        if (is_marked_ref(left)) {
            left = get_unmarked_ref(left);
            if (!node->left.compare_exchange_strong(left, left, std::memory_order_acq_rel)) {
                return remove(node, key);
            }
        }

        if (!left) {
            return false;
        } else {
            return remove(left, key);
        }
    } else {
        Node* right = node->right.load(std::memory_order_acquire);
        if (is_marked_ref(right)) {
            right = get_unmarked_ref(right);
            if (!node->right.compare_exchange_strong(right, right, std::memory_order_acq_rel)) {
                return remove(node, key);
            }
        }

        if (!right) {
            return false;
        } else {
            return remove(right, key);
        }
    }
}

ConcurrentDataStructure::Node* ConcurrentDataStructure::findNode(Node* node, int key) {
    while (node) {
        if (is_marked_ref(node)) {
            node = get_unmarked_ref(node);
            continue;
        }

        if (key < node->val) {
            node = node->left.load(std::memory_order_acquire);
        } else if (key > node->val) {
            node = node->right.load(std::memory_order_acquire);
        } else {
            return node;
        }
    }
    return nullptr;
}

ConcurrentDataStructure::Node* ConcurrentDataStructure::findMinNode(Node* node) {
    while (node) {
        if (is_marked_ref(node)) {
            node = get_unmarked_ref(node);
            continue;
        }

        if (node->left.load(std::memory_order_acquire) == nullptr) {
            return node;
        } else {
            node = node->left.load(std::memory_order_acquire);
        }
    }
    return nullptr;
}