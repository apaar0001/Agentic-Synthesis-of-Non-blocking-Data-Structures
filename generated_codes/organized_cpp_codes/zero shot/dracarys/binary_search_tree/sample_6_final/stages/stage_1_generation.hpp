#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)) {
        head->left = head->right = head;
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* next = get_unmarked_ref(head->left);
            delete head;
            head = next;
        }
    }

    bool contains(int key) {
        Node* parent = nullptr;
        Node* node = head;
        while (node != nullptr) {
            if (is_marked_ref(node)) {
                node = get_unmarked_ref(node->left);
                continue;
            }
            if (node->val == key) {
                return true;
            }
            parent = node;
            if (key < node->val) {
                node = get_unmarked_ref(node->left);
            } else {
                node = get_unmarked_ref(node->right);
            }
        }
        return false;
    }

    bool add(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = head;
            while (node != nullptr) {
                if (is_marked_ref(node)) {
                    node = get_unmarked_ref(node->left);
                    continue;
                }
                if (node->val == key) {
                    return false;
                }
                parent = node;
                if (key < node->val) {
                    node = get_unmarked_ref(node->left);
                } else {
                    node = get_unmarked_ref(node->right);
                }
            }
            Node* newNode = new Node(key);
            newNode->left = newNode->right = nullptr;
            if (key < parent->val) {
                newNode->left = parent->left;
                newNode->right = parent;
                if (parent->left.compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                newNode->left = parent;
                newNode->right = parent->right;
                if (parent->right.compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = head;
            while (node != nullptr) {
                if (is_marked_ref(node)) {
                    node = get_unmarked_ref(node->left);
                    continue;
                }
                if (node->val == key) {
                    break;
                }
                parent = node;
                if (key < node->val) {
                    node = get_unmarked_ref(node->left);
                } else {
                    node = get_unmarked_ref(node->right);
                }
            }
            if (node == nullptr) {
                return false;
            }
            Node* left = get_unmarked_ref(node->left);
            Node* right = get_unmarked_ref(node->right);
            if (left == nullptr) {
                if (node->right.compare_exchange_strong(right, get_marked_ref(right), std::memory_order_acq_rel)) {
                    if (parent->left.load(std::memory_order_relaxed) == node) {
                        parent->left.compare_exchange_strong(node, right, std::memory_order_acq_rel);
                    } else {
                        parent->right.compare_exchange_strong(node, right, std::memory_order_acq_rel);
                    }
                    return true;
                }
            } else if (right == nullptr) {
                if (node->left.compare_exchange_strong(left, get_marked_ref(left), std::memory_order_acq_rel)) {
                    if (parent->left.load(std::memory_order_relaxed) == node) {
                        parent->left.compare_exchange_strong(node, left, std::memory_order_acq_rel);
                    } else {
                        parent->right.compare_exchange_strong(node, left, std::memory_order_acq_rel);
                    }
                    return true;
                }
            } else {
                Node* successor = right;
                Node* successorParent = node;
                while (get_unmarked_ref(successor->left) != nullptr) {
                    successorParent = successor;
                    successor = get_unmarked_ref(successor->left);
                }
                if (successorParent->left.load(std::memory_order_relaxed) == successor) {
                    successorParent->left.compare_exchange_strong(successor, get_marked_ref(successor), std::memory_order_acq_rel);
                } else {
                    successorParent->right.compare_exchange_strong(successor, get_marked_ref(successor), std::memory_order_acq_rel);
                }
                if (node->left.compare_exchange_strong(left, get_marked_ref(left), std::memory_order_acq_rel)) {
                    if (parent->left.load(std::memory_order_relaxed) == node) {
                        parent->left.compare_exchange_strong(node, right, std::memory_order_acq_rel);
                    } else {
                        parent->right.compare_exchange_strong(node, right, std::memory_order_acq_rel);
                    }
                    return true;
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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* head;
};