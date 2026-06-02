#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    std::atomic<Node*> root_;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    void help_remove_marked(Node* parent, bool from_left, Node* victim) {
        Node* u_victim = get_unmarked_ref(victim);
        Node* replacement = nullptr;
        Node* left = u_victim->left.load(std::memory_order_acquire);
        if (left && !is_marked_ref(left)) {
            replacement = left;
        } else {
            Node* right = u_victim->right.load(std::memory_order_acquire);
            if (right && !is_marked_ref(right))
                replacement = right;
        }
        std::atomic<Node*>* link = parent ? (from_left ? &parent->left : &parent->right) : &root_;
        Node* expected = victim;
        Node* desired = replacement;
        link->compare_exchange_strong(expected, desired,
                std::memory_order_acq_rel, std::memory_order_acquire);
    }

public:
    ConcurrentDataStructure() : root_(nullptr) {}

    bool contains(int key) override {
        Node* curr = root_.load(std::memory_order_acquire);
        while (curr) {
            Node* u = get_unmarked_ref(curr);
            if (key == u->val) return true;
            bool go_left = key < u->val;
            Node* child_ptr = go_left ? u->left.load(std::memory_order_acquire)
                                      : u->right.load(std::memory_order_acquire);
            if (is_marked_ref(child_ptr)) return false;
            curr = get_unmarked_ref(child_ptr);
        }
        return false;
    }

    bool add(int key) override {
        restart:
        Node* parent = nullptr;
        Node* curr = root_.load(std::memory_order_acquire);
        bool from_left = false;
        while (curr) {
            if (is_marked_ref(curr)) {
                help_remove_marked(parent, from_left, curr);
                goto restart;
            }
            Node* u = get_unmarked_ref(curr);
            if (key == u->val) return false;
            parent = u;
            if (key < u->val) {
                from_left = true;
                curr = u->left.load(std::memory_order_acquire);
            } else {
                from_left = false;
                curr = u->right.load(std::memory_order_acquire);
            }
        }
        Node* new_node = new Node(key);
        if (!parent) {
            Node* expected = nullptr;
            if (root_.compare_exchange_strong(expected, new_node,
                    std::memory_order_release, std::memory_order_relaxed))
                return true;
            else
                goto restart;
        }
        std::atomic<Node*>* child_ptr = from_left ? &parent->left : &parent->right;
        Node* expected = nullptr;
        if (child_ptr->compare_exchange_strong(expected, new_node,
                std::memory_order_release, std::memory_order_relaxed))
            return true;
        else
            goto restart;
    }

    bool remove(int key) override {
        restart:
        Node* parent = nullptr;
        Node* curr = root_.load(std::memory_order_acquire);
        bool from_left = false;
        while (curr) {
            if (is_marked_ref(curr)) {
                help_remove_marked(parent, from_left, curr);
                goto restart;
            }
            Node* u = get_unmarked_ref(curr);
            if (key == u->val) {
                std::atomic<Node*>* link = parent ? (from_left ? &parent->left : &parent->right)
                                                  : &root_;
                Node* expected = curr;
                Node* desired = get_marked_ref(curr);
                if (link->compare_exchange_strong(expected, desired,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    return true;
                }
                goto restart;
            }
            parent = u;
            if (key < u->val) {
                from_left = true;
                curr = u->left.load(std::memory_order_acquire);
            } else {
                from_left = false;
                curr = u->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
};