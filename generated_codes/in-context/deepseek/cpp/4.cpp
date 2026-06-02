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

        Node(int v)
            : val(v), left(nullptr), right(nullptr) {}
    };

    Node* root;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    struct SearchResult {
        Node* parent;
        Node* node;
        bool is_left;
    };

    bool try_cleanup(Node* parent, Node* node, bool is_left) {
        if (!parent || !node) {
            return false;
        }

        std::atomic<Node*>* link =
            is_left ? &parent->left : &parent->right;

        Node* expected = node;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

        Node* replacement = nullptr;

        if (!left) {
            replacement = right;
        } else if (!right) {
            replacement = left;
        } else {
            return false;
        }

        return link->compare_exchange_strong(
            expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    SearchResult search(int key) {
        retry:
        Node* parent = root;
        Node* curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));
        bool is_left = false;

        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left) || is_marked_ref(right)) {
                if (parent != root) {
                    try_cleanup(parent, curr, is_left);
                }
                goto retry;
            }

            if (key == curr->val) {
                return { parent, curr, is_left };
            }

            parent = curr;

            if (key < curr->val) {
                curr = get_unmarked_ref(left);
                is_left = true;
            } else {
                curr = get_unmarked_ref(right);
                is_left = false;
            }
        }

        return { parent, nullptr, is_left };
    }

    void destroy(Node* node) {
        if (!node) {
            return;
        }

        Node* left = get_unmarked_ref(node->left.load(std::memory_order_relaxed));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_relaxed));

        destroy(left);
        destroy(right);

        delete node;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MIN);
        Node* max_node = new Node(INT_MAX);
        root->right.store(max_node, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));

        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (!is_marked_ref(left) &&
                !is_marked_ref(right) &&
                curr->val == key) {
                return true;
            }

            if (key < curr->val) {
                curr = get_unmarked_ref(left);
            } else {
                curr = get_unmarked_ref(right);
            }
        }

        return false;
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);

            if (res.node) {
                return false;
            }

            Node* parent = res.parent;
            Node* new_node = new Node(key);

            std::atomic<Node*>* link =
                res.is_left ? &parent->left : &parent->right;

            Node* expected = nullptr;

            if (link->compare_exchange_strong(
                    expected,
                    new_node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);

            Node* parent = res.parent;
            Node* node = res.node;

            if (!node) {
                return false;
            }

            Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

            if (left && right) {
                Node* succ_parent = node;
                Node* succ = right;
                bool succ_left = false;

                while (true) {
                    Node* next = get_unmarked_ref(
                        succ->left.load(std::memory_order_acquire));

                    if (!next) {
                        break;
                    }

                    succ_parent = succ;
                    succ = next;
                    succ_left = true;
                }

                int succ_val = succ->val;

                if (remove(succ_val)) {
                    node->val = succ_val;
                    return true;
                }

                continue;
            }

            Node* marked = get_marked_ref(left);

            Node* expected_left = left;

            if (!node->left.compare_exchange_strong(
                    expected_left,
                    marked,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            Node* replacement = left ? left : right;

            std::atomic<Node*>* link =
                res.is_left ? &parent->left : &parent->right;

            Node* expected = node;

            if (link->compare_exchange_strong(
                    expected,
                    replacement,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
    }
};
