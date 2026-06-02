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

    void help_remove_marked(Node* parent, std::atomic<Node*>* field) {
        Node* marked = field->load(std::memory_order_acquire);
        if (!is_marked_ref(marked)) return;
        Node* node = get_unmarked_ref(marked);
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        Node* replacement = nullptr;
        if (!get_unmarked_ref(left)) {
            replacement = get_unmarked_ref(right);
        } else if (!get_unmarked_ref(right)) {
            replacement = get_unmarked_ref(left);
        }
        if (replacement || !left && !right) {
            Node* expected = marked;
            if (field->compare_exchange_strong(expected, get_marked_ref(replacement),
                    std::memory_order_release, std::memory_order_relaxed)) {
                // Successfully helped remove marked node
            }
        }
    }

    bool try_remove_node(Node* parent, std::atomic<Node*>* field, Node* node) {
        Node* expected = node;
        Node* desired = get_marked_ref(node);
        if (!field->compare_exchange_strong(expected, desired,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return false;
        }
        // Node has been marked
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        Node* replacement = nullptr;
        if (!get_unmarked_ref(left)) {
            replacement = get_unmarked_ref(right);
        } else if (!get_unmarked_ref(right)) {
            replacement = get_unmarked_ref(left);
        }
        if (parent) {
            Node* exp = desired;
            if (!field->compare_exchange_strong(exp, get_marked_ref(replacement),
                    std::memory_order_release, std::memory_order_relaxed)) {
                return false;
            }
        } else {
            Node* exp = desired;
            if (!root_.compare_exchange_strong(exp, get_marked_ref(replacement),
                    std::memory_order_release, std::memory_order_relaxed)) {
                return false;
            }
        }
        return true;
    }

    Node* find_successor(Node* node) {
        Node* succ = node->right.load(std::memory_order_acquire);
        while (succ && !is_marked_ref(succ)) {
            Node* left = succ->left.load(std::memory_order_acquire);
            if (!get_unmarked_ref(left)) break;
            succ = get_unmarked_ref(left);
        }
        return get_unmarked_ref(succ);
    }

public:
    ConcurrentDataStructure() : root_(nullptr) {}
    ~ConcurrentDataStructure() override {
        Node* curr = root_.load(std::memory_order_acquire);
        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);
            clear(get_unmarked_ref(left));
            clear(get_unmarked_ref(right));
            delete curr;
            curr = get_unmarked_ref(root_.load(std::memory_order_acquire));
        }
    }

    void clear(Node* node) {
        if (!node) return;
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        clear(get_unmarked_ref(left));
        clear(get_unmarked_ref(right));
        delete node;
    }

    bool contains(int key) override {
        retry:
        Node* curr = root_.load(std::memory_order_acquire);
        while (curr) {
            if (is_marked_ref(curr)) {
                goto retry;
            }
            if (key == curr->val) {
                return true;
            }
            std::atomic<Node*>* child = (key < curr->val) ? &(curr->left) : &(curr->right);
            Node* next = child->load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                goto retry;
            }
            curr = get_unmarked_ref(next);
        }
        return false;
    }

    bool add(int key) override {
        retry:
        Node* pred = nullptr;
        std::atomic<Node*>* pred_field = nullptr;
        Node* curr = root_.load(std::memory_order_acquire);
        while (curr) {
            if (is_marked_ref(curr)) {
                goto retry;
            }
            if (key == curr->val) {
                return false;
            }
            pred = curr;
            pred_field = (key < curr->val) ? &(curr->left) : &(curr->right);
            Node* next = pred_field->load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                goto retry;
            }
            curr = get_unmarked_ref(next);
        }
        Node* newnode = new Node(key);
        Node* expected = nullptr;
        if (pred_field->compare_exchange_strong(
                expected, newnode,
                std::memory_order_release, std::memory_order_relaxed)) {
            return true;
        } else {
            delete newnode;
            goto retry;
        }
    }

    bool remove(int key) override {
        retry:
        Node* pred = nullptr;
        std::atomic<Node*>* pred_field = nullptr;
        Node* curr = root_.load(std::memory_order_acquire);
        while (curr) {
            if (is_marked_ref(curr)) {
                goto retry;
            }
            if (key == curr->val) {
                break;
            }
            pred = curr;
            pred_field = (key < curr->val) ? &(curr->left) : &(curr->right);
            Node* next = pred_field->load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                goto retry;
            }
            curr = get_unmarked_ref(next);
        }
        if (!curr) {
            return false;
        }
        if (!try_remove_node(pred, pred_field, curr)) {
            goto retry;
        }
        return true;
    }
};