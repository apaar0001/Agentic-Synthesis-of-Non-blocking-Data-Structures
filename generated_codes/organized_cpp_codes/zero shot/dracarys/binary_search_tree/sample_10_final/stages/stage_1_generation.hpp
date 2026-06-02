#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MAX)) {
        head->left = head;
        head->right = tail;
        tail->left = head;
        tail->right = tail;
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != tail) {
            Node* next = get_unmarked_ref(current->right);
            delete current;
            current = next;
        }
        delete tail;
    }

    bool contains(int key) {
        while (true) {
            Node* parent = head;
            Node* current = parent->right;
            while (current != tail) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current->right);
                    continue;
                }
                if (current->val == key) {
                    return true;
                }
                if (key < current->val) {
                    parent = current;
                    current = current->left;
                } else {
                    parent = current;
                    current = current->right;
                }
            }
            return false;
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = head;
            Node* current = parent->right;
            while (current != tail) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current->right);
                    continue;
                }
                if (current->val == key) {
                    return false;
                }
                if (key < current->val) {
                    parent = current;
                    current = current->left;
                } else {
                    parent = current;
                    current = current->right;
                }
            }
            Node* new_node = new Node(key);
            new_node->left = parent;
            new_node->right = current;
            if (parent->right.compare_exchange_strong(current, new_node, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = head;
            Node* current = parent->right;
            while (current != tail) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current->right);
                    continue;
                }
                if (current->val == key) {
                    Node* marked_current = get_marked_ref(current);
                    if (current->right.compare_exchange_strong(current->right, marked_current, std::memory_order_acq_rel)) {
                        Node* new_current = get_unmarked_ref(current->right);
                        if (parent->right.compare_exchange_strong(current, new_current, std::memory_order_acq_rel)) {
                            delete current;
                            return true;
                        }
                    }
                }
                if (key < current->val) {
                    parent = current;
                    current = current->left;
                } else {
                    parent = current;
                    current = current->right;
                }
            }
            return false;
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
    Node* tail;
};