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
    };

    ConcurrentDataStructure() : head(new Node{INT_MIN, nullptr, nullptr}) {
        head->left = head;
        head->right = new Node{INT_MAX, nullptr, nullptr};
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* next = get_unmarked_ref(head->left);
            delete head;
            head = next;
        }
    }

    bool contains(int key) {
        Node* curr = head;
        while (true) {
            Node* next = get_unmarked_ref(curr->left);
            if (next->val >= key) {
                if (next->val == key) return true;
                curr = next;
            } else {
                next = get_unmarked_ref(curr->right);
                if (next->val <= key) {
                    if (next->val == key) return true;
                    curr = next;
                } else {
                    return false;
                }
            }
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* target = search_helper(key, parent);
            if (target->val == key) return false;
            Node* new_node = new Node{key, nullptr, nullptr};
            if (target->val > key) {
                new_node->left = get_unmarked_ref(parent->left);
                new_node->right = target;
                if (parent->left.compare_exchange_strong(new_node->left, new_node, std::memory_order_acq_rel)) return true;
            } else {
                new_node->left = target;
                new_node->right = get_unmarked_ref(parent->right);
                if (parent->right.compare_exchange_strong(new_node->right, new_node, std::memory_order_acq_rel)) return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* target = search_helper(key, parent);
            if (target->val != key) return false;
            if (target->left.load(std::memory_order_acquire) == nullptr || target->right.load(std::memory_order_acquire) == nullptr) {
                Node* replacement = target->left.load(std::memory_order_acquire) != nullptr ? target->left : target->right;
                if (target->val > key) {
                    if (parent->left.compare_exchange_strong(target, replacement, std::memory_order_acq_rel)) {
                        delete target;
                        return true;
                    }
                } else {
                    if (parent->right.compare_exchange_strong(target, replacement, std::memory_order_acq_rel)) {
                        delete target;
                        return true;
                    }
                }
            } else {
                Node* marked_target = get_marked_ref(target);
                if (target->left.compare_exchange_strong(target, marked_target, std::memory_order_acq_rel)) {
                    Node* replacement = target->left.load(std::memory_order_acquire) != nullptr ? target->left : target->right;
                    if (target->val > key) {
                        if (parent->left.compare_exchange_strong(marked_target, replacement, std::memory_order_acq_rel)) {
                            delete target;
                            return true;
                        }
                    } else {
                        if (parent->right.compare_exchange_strong(marked_target, replacement, std::memory_order_acq_rel)) {
                            delete target;
                            return true;
                        }
                    }
                }
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

    Node* search_helper(int key, Node*& parent) {
        Node* curr = head;
        while (true) {
            Node* next = get_unmarked_ref(curr->left);
            if (next->val >= key) {
                if (next->val == key) return next;
                parent = curr;
                curr = next;
            } else {
                next = get_unmarked_ref(curr->right);
                if (next->val <= key) {
                    if (next->val == key) return next;
                    parent = curr;
                    curr = next;
                } else {
                    return next;
                }
            }
        }
    }

    Node* head;
};