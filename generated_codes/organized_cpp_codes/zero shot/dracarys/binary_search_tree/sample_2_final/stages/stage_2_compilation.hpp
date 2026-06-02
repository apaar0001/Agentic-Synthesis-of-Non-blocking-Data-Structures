#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        Node* min = new Node(INT_MIN);
        Node* max = new Node(INT_MAX);
        min->left = min;
        min->right = max;
        max->left = min;
        max->right = max;
        head = min;
    }

    ~ConcurrentDataStructure() {
        Node* node = head;
        while (node != nullptr) {
            Node* next = get_unmarked_ref(node->right);
            delete node;
            node = next;
        }
    }

    bool contains(int key) {
        Node* node = head;
        while (true) {
            Node* next = get_unmarked_ref(node->right);
            if (next->val == key) {
                return true;
            }
            if (next->val > key) {
                node = get_unmarked_ref(node->left);
                if (node == head) {
                    return false;
                }
            } else {
                node = next;
            }
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = search(parent, key);
            if (node->val == key) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->left = node;
            newNode->right = node;
            if (parent->left.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = search(parent, key);
            if (node->val != key) {
                return false;
            }
            Node* marked = get_marked_ref(node->right);
            if (node->right.compare_exchange_strong(node->right, marked, std::memory_order_acq_rel)) {
                if (parent->left.compare_exchange_strong(node, get_unmarked_ref(node->right), std::memory_order_acq_rel)) {
                    delete node;
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

    Node* search(Node*& parent, int key) {
        Node* node = head;
        while (true) {
            Node* next = get_unmarked_ref(node->right);
            if (next->val == key) {
                return next;
            }
            if (next->val > key) {
                node = get_unmarked_ref(node->left);
                if (node == head) {
                    return node;
                }
            } else {
                parent = node;
                node = next;
            }
        }
    }

    Node* head;
};