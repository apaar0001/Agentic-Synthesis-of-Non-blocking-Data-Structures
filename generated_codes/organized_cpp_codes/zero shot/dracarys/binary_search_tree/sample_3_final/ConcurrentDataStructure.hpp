#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN)) {
        head->left = head;
        head->right = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() {
        destroyTree(head);
    }

    bool contains(int key) {
        Node* node = head;
        while (true) {
            Node* next = node->left.load(std::memory_order_acquire);
            if (next->val >= key) {
                if (next->val == key) {
                    return !is_marked_ref(next);
                }
                node = next;
            } else {
                next = node->right.load(std::memory_order_acquire);
                if (next->val <= key) {
                    if (next->val == key) {
                        return !is_marked_ref(next);
                    }
                    node = next;
                } else {
                    return false;
                }
            }
        }
    }

    bool add(int key) {
        Node* parent = head;
        Node* node = parent->left.load(std::memory_order_acquire);
        while (true) {
            if (node->val == key) {
                return false;
            }
            if (node->val < key) {
                if (node->right.load(std::memory_order_acquire) == nullptr) {
                    Node* newNode = new Node(key);
                    if (node->right.compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    parent = node;
                    node = node->right.load(std::memory_order_acquire);
                }
            } else {
                if (node->left.load(std::memory_order_acquire) == nullptr) {
                    Node* newNode = new Node(key);
                    if (node->left.compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    parent = node;
                    node = node->left.load(std::memory_order_acquire);
                }
            }
        }
    }

    bool remove(int key) {
        Node* parent = head;
        Node* node = parent->left.load(std::memory_order_acquire);
        while (true) {
            if (node->val == key) {
                if (is_marked_ref(node)) {
                    return false;
                }
                Node* markedNode = get_marked_ref(node);
                if (node->left.compare_exchange_strong(node, markedNode, std::memory_order_acq_rel)) {
                    Node* replacement = node->left.load(std::memory_order_acquire);
                    if (replacement == nullptr) {
                        parent->left.compare_exchange_strong(markedNode, node->right.load(std::memory_order_acquire), std::memory_order_acq_rel);
                    } else {
                        parent->left.compare_exchange_strong(markedNode, replacement, std::memory_order_acq_rel);
                    }
                    delete node;
                    return true;
                }
            } else if (node->val < key) {
                parent = node;
                node = node->right.load(std::memory_order_acquire);
            } else {
                parent = node;
                node = node->left.load(std::memory_order_acquire);
            }
        }
    }

private:
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    void destroyTree(Node* node) {
        if (node == nullptr) {
            return;
        }
        destroyTree(get_unmarked_ref(node->left.load(std::memory_order_acquire)));
        destroyTree(get_unmarked_ref(node->right.load(std::memory_order_acquire)));
        delete node;
    }

    Node* head;
};