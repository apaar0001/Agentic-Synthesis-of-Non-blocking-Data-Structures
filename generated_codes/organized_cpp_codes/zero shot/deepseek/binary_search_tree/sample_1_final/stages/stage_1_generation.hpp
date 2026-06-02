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

    std::atomic<Node*> root;

    struct SearchResult {
        Node* parent;
        Node* target;
        Node* left_child;
        Node* right_child;
    };

    SearchResult search(int key) {
        SearchResult result{nullptr, nullptr, nullptr, nullptr};
        Node* parent = nullptr;
        Node* curr = root.load(std::memory_order_acquire);
        Node* left_child = nullptr;
        Node* right_child = nullptr;

        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == nullptr) break;

            if (is_marked_ref(curr)) {
                if (parent == nullptr) {
                    Node* next = unmarked_curr;
                    root.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    curr = root.load(std::memory_order_acquire);
                    continue;
                }
                Node* next = unmarked_curr;
                if (parent->left.load(std::memory_order_relaxed) == curr) {
                    parent->left.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                } else {
                    parent->right.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                }
                curr = next;
                continue;
            }

            if (unmarked_curr->val == key) {
                result.parent = parent;
                result.target = curr;
                result.left_child = get_unmarked_ref(unmarked_curr->left.load(std::memory_order_acquire));
                result.right_child = get_unmarked_ref(unmarked_curr->right.load(std::memory_order_acquire));
                return result;
            }

            parent = curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }

        result.parent = parent;
        return result;
    }

    void cleanup(Node* node) {
        if (node == nullptr) return;
        Node* unmarked = get_unmarked_ref(node);
        cleanup(unmarked->left.load(std::memory_order_relaxed));
        cleanup(unmarked->right.load(std::memory_order_relaxed));
        delete unmarked;
    }

public:
    ConcurrentDataStructure() {
        root.store(nullptr, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        cleanup(root.load(std::memory_order_relaxed));
    }

    bool contains(int key) override {
        SearchResult res = search(key);
        return res.target != nullptr && !is_marked_ref(res.target);
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (res.target != nullptr) {
                if (is_marked_ref(res.target)) continue;
                return false;
            }

            Node* new_node = new Node(key);
            if (res.parent == nullptr) {
                if (root.compare_exchange_strong(res.parent, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
                continue;
            }

            Node* unmarked_parent = get_unmarked_ref(res.parent);
            if (is_marked_ref(res.parent)) continue;

            std::atomic<Node*>* child_ptr;
            if (key < unmarked_parent->val) {
                child_ptr = &unmarked_parent->left;
            } else {
                child_ptr = &unmarked_parent->right;
            }

            Node* expected = child_ptr->load(std::memory_order_acquire);
            if (expected != nullptr && is_marked_ref(expected)) continue;

            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (res.target == nullptr || is_marked_ref(res.target)) {
                return false;
            }

            Node* unmarked_target = get_unmarked_ref(res.target);
            Node* marked_target = get_marked_ref(res.target);

            if (res.target->left.compare_exchange_strong(res.left_child, marked_target, std::memory_order_acq_rel)) {
                if (res.parent == nullptr) {
                    Node* right = unmarked_target->right.load(std::memory_order_acquire);
                    root.compare_exchange_strong(res.target, right, std::memory_order_acq_rel);
                } else {
                    Node* unmarked_parent = get_unmarked_ref(res.parent);
                    if (is_marked_ref(res.parent)) continue;

                    std::atomic<Node*>* child_ptr;
                    if (unmarked_target->val < unmarked_parent->val) {
                        child_ptr = &unmarked_parent->left;
                    } else {
                        child_ptr = &unmarked_parent->right;
                    }

                    Node* right = unmarked_target->right.load(std::memory_order_acquire);
                    child_ptr->compare_exchange_strong(res.target, right, std::memory_order_acq_rel);
                }
                return true;
            }
        }
    }
};