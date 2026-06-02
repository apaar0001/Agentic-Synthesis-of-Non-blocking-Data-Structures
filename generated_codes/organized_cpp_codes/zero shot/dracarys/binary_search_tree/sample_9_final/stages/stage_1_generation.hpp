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
        while (head != head->left) {
            Node* next = head->left;
            delete head;
            head = next;
        }
        delete head;
    }

    bool contains(int key) {
        while (true) {
            Node* parent = head;
            Node* node = parent->right;
            while (node->val < key) {
                parent = node;
                node = node->right;
            }
            if (node->val == key && !is_marked_ref(node)) {
                return true;
            }
            if (parent->right.load(std::memory_order_relaxed) != node) {
                continue;
            }
            return false;
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = head;
            Node* node = parent->right;
            while (node->val < key) {
                parent = node;
                node = node->right;
            }
            if (node->val == key) {
                if (!is_marked_ref(node)) {
                    return false;
                }
                if (parent->right.compare_exchange_strong(node, get_unmarked_ref(node), std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                Node* newNode = new Node(key);
                newNode->left = node;
                newNode->right = get_marked_ref(node);
                if (parent->right.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = head;
            Node* node = parent->right;
            while (node->val < key) {
                parent = node;
                node = node->right;
            }
            if (node->val != key || is_marked_ref(node)) {
                return false;
            }
            Node* markedNode = get_marked_ref(node);
            if (node->right.compare_exchange_strong(node->right, markedNode, std::memory_order_acq_rel)) {
                if (parent->right.compare_exchange_strong(node, node->right, std::memory_order_acq_rel)) {
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

        Node(int val) : val(val) {}
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