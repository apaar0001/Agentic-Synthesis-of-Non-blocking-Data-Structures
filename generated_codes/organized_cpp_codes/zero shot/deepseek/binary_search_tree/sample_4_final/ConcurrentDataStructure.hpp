#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int v, Node* l = nullptr, Node* r = nullptr) : val(v), left(l), right(r) {}
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

    Node* sentinel_min;
    Node* sentinel_max;
    std::atomic<Node*> root;

    void cleanup(Node* node) {
        if (!node) return;
        Node* unmarked_left = get_unmarked_ref(node->left.load(std::memory_order_relaxed));
        Node* unmarked_right = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
        cleanup(unmarked_left);
        cleanup(unmarked_right);
        if (node != sentinel_min && node != sentinel_max) {
            delete node;
        }
    }

    bool search(int key, Node** parent, Node** target, Node** grandparent = nullptr) {
        Node* prev = nullptr;
        Node* curr = root.load(std::memory_order_acquire);
        Node* prev_prev = nullptr;

        while (true) {
            if (curr == nullptr) {
                *parent = prev;
                *target = nullptr;
                if (grandparent) *grandparent = prev_prev;
                return false;
            }

            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* unmarked_left = get_unmarked_ref(unmarked_curr->left.load(std::memory_order_acquire));
            Node* unmarked_right = get_unmarked_ref(unmarked_curr->right.load(std::memory_order_acquire));

            if (unmarked_curr->val == key) {
                *parent = prev;
                *target = unmarked_curr;
                if (grandparent) *grandparent = prev_prev;
                return true;
            }

            prev_prev = prev;
            prev = unmarked_curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_left;
            } else {
                curr = unmarked_right;
            }
        }
    }

    bool help_remove(Node* parent, Node* target) {
        Node* target_left = target->left.load(std::memory_order_acquire);
        Node* target_right = target->right.load(std::memory_order_acquire);

        Node* marked_left = get_marked_ref(target_left);
        Node* marked_right = get_marked_ref(target_right);

        target->left.compare_exchange_strong(target_left, marked_left,
            std::memory_order_acq_rel, std::memory_order_acquire);
        target->right.compare_exchange_strong(target_right, marked_right,
            std::memory_order_acq_rel, std::memory_order_acquire);

        Node* unmarked_target = target;
        Node* successor = nullptr;

        if (target_left != nullptr && !is_marked_ref(target_left)) {
            Node* left_unmarked = get_unmarked_ref(target_left);
            Node* rightmost = left_unmarked;
            Node* rightmost_right = rightmost->right.load(std::memory_order_acquire);
            while (rightmost_right != nullptr && !is_marked_ref(rightmost_right)) {
                rightmost = get_unmarked_ref(rightmost_right);
                rightmost_right = rightmost->right.load(std::memory_order_acquire);
            }
            successor = rightmost;
        } else if (target_right != nullptr && !is_marked_ref(target_right)) {
            successor = get_unmarked_ref(target_right);
        }

        Node* new_child = nullptr;
        if (successor) {
            Node* succ_left = successor->left.load(std::memory_order_acquire);
            Node* succ_right = successor->right.load(std::memory_order_acquire);
            if (succ_left != nullptr && !is_marked_ref(succ_left)) {
                new_child = get_unmarked_ref(succ_left);
            } else if (succ_right != nullptr && !is_marked_ref(succ_right)) {
                new_child = get_unmarked_ref(succ_right);
            }
        }

        Node* parent_left = parent->left.load(std::memory_order_acquire);
        Node* parent_right = parent->right.load(std::memory_order_acquire);

        if (parent_left == unmarked_target) {
            return parent->left.compare_exchange_strong(parent_left, new_child,
                std::memory_order_acq_rel, std::memory_order_acquire);
        } else if (parent_right == unmarked_target) {
            return parent->right.compare_exchange_strong(parent_right, new_child,
                std::memory_order_acq_rel, std::memory_order_acquire);
        }
        return false;
    }

public:
    ConcurrentDataStructure() {
        sentinel_min = new Node(INT_MIN);
        sentinel_max = new Node(INT_MAX);
        sentinel_min->right.store(sentinel_max, std::memory_order_relaxed);
        sentinel_max->left.store(sentinel_min, std::memory_order_relaxed);
        root.store(sentinel_min, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_relaxed);
        cleanup(r);
        delete sentinel_min;
        delete sentinel_max;
    }

    bool contains(int key) override {
        Node* parent;
        Node* target;
        bool found = search(key, &parent, &target);
        if (!found) return false;
        if (target == nullptr) return false;
        Node* target_left = target->left.load(std::memory_order_acquire);
        Node* target_right = target->right.load(std::memory_order_acquire);
        return !is_marked_ref(target_left) && !is_marked_ref(target_right);
    }

    bool add(int key) override {
        while (true) {
            Node* parent;
            Node* target;
            if (search(key, &parent, &target)) {
                if (target == nullptr) return false;
                Node* target_left = target->left.load(std::memory_order_acquire);
                Node* target_right = target->right.load(std::memory_order_acquire);
                if (is_marked_ref(target_left) || is_marked_ref(target_right)) {
                    help_remove(parent, target);
                    continue;
                }
                return false;
            }

            Node* new_node = new Node(key);
            Node* parent_left = parent->left.load(std::memory_order_acquire);
            Node* parent_right = parent->right.load(std::memory_order_acquire);

            if (key < parent->val) {
                if (parent_left != nullptr && !is_marked_ref(parent_left)) {
                    continue;
                }
                new_node->left.store(parent_left, std::memory_order_relaxed);
                new_node->right.store(parent, std::memory_order_relaxed);
                if (parent->left.compare_exchange_strong(parent_left, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            } else {
                if (parent_right != nullptr && !is_marked_ref(parent_right)) {
                    continue;
                }
                new_node->left.store(parent, std::memory_order_relaxed);
                new_node->right.store(parent_right, std::memory_order_relaxed);
                if (parent->right.compare_exchange_strong(parent_right, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            }

            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* grandparent;
            Node* parent;
            Node* target;
            if (!search(key, &parent, &target, &grandparent)) {
                return false;
            }
            if (target == nullptr) return false;

            Node* target_left = target->left.load(std::memory_order_acquire);
            Node* target_right = target->right.load(std::memory_order_acquire);

            if (is_marked_ref(target_left) || is_marked_ref(target_right)) {
                if (grandparent) {
                    help_remove(grandparent, parent);
                }
                continue;
            }

            Node* marked_left = get_marked_ref(target_left);
            Node* marked_right = get_marked_ref(target_right);

            if (target->left.compare_exchange_strong(target_left, marked_left,
                std::memory_order_acq_rel, std::memory_order_acquire) ||
                target->right.compare_exchange_strong(target_right, marked_right,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                help_remove(parent, target);
                return true;
            }
        }
    }
};