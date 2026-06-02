#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root(new Node(INT_MIN)) {}

    bool contains(int key) override {
        Node* node = find(root, key);
        return node != nullptr && !is_marked_ref(node);
    }

    bool add(int key) override {
        while (true) {
            Node* node = find(root, key);
            if (node != nullptr) {
                return false;
            }
            Node* newNode = new Node(key);
            Node* parent = findParent(root, key);
            if (parent == nullptr) {
                if (root->left.load(std::memory_order_acquire) == nullptr) {
                    Node* expected = nullptr;
                    if (root->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        return true;
                    }
                } else {
                    if (root->right.load(std::memory_order_acquire) == nullptr) {
                        Node* expected = nullptr;
                        if (root->right.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            return true;
                        }
                    }
                }
            } else {
                if (key < parent->val) {
                    if (parent->left.load(std::memory_order_acquire) == nullptr) {
                        Node* expected = nullptr;
                        if (parent->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            return true;
                        }
                    }
                } else {
                    if (parent->right.load(std::memory_order_acquire) == nullptr) {
                        Node* expected = nullptr;
                        if (parent->right.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            return true;
                        }
                    }
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* node = find(root, key);
            if (node == nullptr) {
                return false;
            }
            if (is_marked_ref(node)) {
                return false;
            }
            Node* parent = findParent(root, key);
            if (parent == nullptr) {
                if (node == root) {
                    return false;
                }
            } else {
                if (key < parent->val) {
                    if (parent->left.load(std::memory_order_acquire) == node) {
                        Node* expected = node;
                        if (parent->left.compare_exchange_strong(expected, get_marked_ref(node), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            // Node has been marked
                            if (node->left.load(std::memory_order_acquire) == nullptr) {
                                Node* expected2 = get_marked_ref(node);
                                if (parent->left.compare_exchange_strong(expected2, node->right.load(std::memory_order_acquire), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                    delete node;
                                    return true;
                                }
                            } else if (node->right.load(std::memory_order_acquire) == nullptr) {
                                Node* expected2 = get_marked_ref(node);
                                if (parent->left.compare_exchange_strong(expected2, node->left.load(std::memory_order_acquire), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                    delete node;
                                    return true;
                                }
                            } else {
                                Node* minNode = findMin(node->right.load(std::memory_order_acquire));
                                if (minNode != nullptr) {
                                    if (minNode->left.load(std::memory_order_acquire) == nullptr) {
                                        Node* expected2 = nullptr;
                                        if (minNode->right.compare_exchange_strong(expected2, get_marked_ref(node), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                            Node* expected3 = get_marked_ref(node);
                                            if (parent->left.compare_exchange_strong(expected3, minNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                                delete node;
                                                return true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                } else {
                    if (parent->right.load(std::memory_order_acquire) == node) {
                        Node* expected = node;
                        if (parent->right.compare_exchange_strong(expected, get_marked_ref(node), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            // Node has been marked
                            if (node->left.load(std::memory_order_acquire) == nullptr) {
                                Node* expected2 = get_marked_ref(node);
                                if (parent->right.compare_exchange_strong(expected2, node->right.load(std::memory_order_acquire), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                    delete node;
                                    return true;
                                }
                            } else if (node->right.load(std::memory_order_acquire) == nullptr) {
                                Node* expected2 = get_marked_ref(node);
                                if (parent->right.compare_exchange_strong(expected2, node->left.load(std::memory_order_acquire), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                    delete node;
                                    return true;
                                }
                            } else {
                                Node* minNode = findMin(node->right.load(std::memory_order_acquire));
                                if (minNode != nullptr) {
                                    if (minNode->left.load(std::memory_order_acquire) == nullptr) {
                                        Node* expected2 = nullptr;
                                        if (minNode->right.compare_exchange_strong(expected2, get_marked_ref(node), std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                            Node* expected3 = get_marked_ref(node);
                                            if (parent->right.compare_exchange_strong(expected3, minNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                                delete node;
                                                return true;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
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

    Node* find(Node* node, int key) {
        while (node != nullptr) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
            } else if (key < node->val) {
                node = node->left.load(std::memory_order_acquire);
            } else if (key > node->val) {
                node = node->right.load(std::memory_order_acquire);
            } else {
                return node;
            }
        }
        return nullptr;
    }

    Node* findParent(Node* node, int key) {
        while (node != nullptr) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
            } else if (key < node->val) {
                if (node->left.load(std::memory_order_acquire) != nullptr) {
                    if (key < node->left.load(std::memory_order_acquire)->val) {
                        node = node->left.load(std::memory_order_acquire);
                    } else {
                        return node;
                    }
                } else {
                    return node;
                }
            } else if (key > node->val) {
                if (node->right.load(std::memory_order_acquire) != nullptr) {
                    if (key > node->right.load(std::memory_order_acquire)->val) {
                        node = node->right.load(std::memory_order_acquire);
                    } else {
                        return node;
                    }
                } else {
                    return node;
                }
            } else {
                return node;
            }
        }
        return nullptr;
    }

    Node* findMin(Node* node) {
        while (node != nullptr) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node);
            } else if (node->left.load(std::memory_order_acquire) != nullptr) {
                node = node->left.load(std::memory_order_acquire);
            } else {
                return node;
            }
        }
        return nullptr;
    }
};