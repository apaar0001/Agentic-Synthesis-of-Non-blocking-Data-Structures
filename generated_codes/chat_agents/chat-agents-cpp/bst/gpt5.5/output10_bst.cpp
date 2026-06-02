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

        explicit Node(int v)
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
        Node* curr;
        bool is_left;
    };

    bool cleanup_child(Node* parent, bool is_left, Node* expected) {
        Node* clean = get_unmarked_ref(expected);

        Node* replacement = nullptr;

        Node* left = get_unmarked_ref(clean->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(clean->right.load(std::memory_order_acquire));

        if (left && right) {
            return false;
        }

        replacement = left ? left : right;

        std::atomic<Node*>* child_ptr;

        if (parent == nullptr) {
            child_ptr = &reinterpret_cast<std::atomic<Node*>&>(root);
        } else {
            child_ptr = is_left ? &parent->left : &parent->right;
        }

        Node* marked_expected = expected;

        return child_ptr->compare_exchange_strong(
            marked_expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    SearchResult search(int key) {
    retry:
        Node* parent = nullptr;
        Node* curr = root;
        bool is_left = false;

        while (curr) {
            curr = get_unmarked_ref(curr);

            if (!curr) {
                break;
            }

            if (curr->val == key) {
                return {parent, curr, is_left};
            }

            parent = curr;

            if (key < curr->val) {
                Node* next = curr->left.load(std::memory_order_acquire);

                if (is_marked_ref(next)) {
                    cleanup_child(parent, true, next);
                    goto retry;
                }

                curr = next;
                is_left = true;
            } else {
                Node* next = curr->right.load(std::memory_order_acquire);

                if (is_marked_ref(next)) {
                    cleanup_child(parent, false, next);
                    goto retry;
                }

                curr = next;
                is_left = false;
            }
        }

        return {parent, nullptr, is_left};
    }

    void destroy(Node* node) {
        if (!node) {
            return;
        }

        node = get_unmarked_ref(node);

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
        root = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = root;

        while (curr) {
            curr = get_unmarked_ref(curr);

            if (!curr) {
                return false;
            }

            if (curr->val == key) {
                return true;
            }

            if (key < curr->val) {
                Node* next = curr->left.load(std::memory_order_acquire);

                if (is_marked_ref(next)) {
                    return false;
                }

                curr = next;
            } else {
                Node* next = curr->right.load(std::memory_order_acquire);

                if (is_marked_ref(next)) {
                    return false;
                }

                curr = next;
            }
        }

        return false;
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);

            if (res.curr) {
                return false;
            }

            Node* node = new Node(key);

            if (res.parent == nullptr) {
                Node* expected = nullptr;

                if (__atomic_compare_exchange_n(
                        &root,
                        &expected,
                        node,
                        false,
                        __ATOMIC_ACQ_REL,
                        __ATOMIC_ACQUIRE)) {
                    return true;
                }

                delete node;
                continue;
            }

            std::atomic<Node*>* child_ptr =
                res.is_left ? &res.parent->left : &res.parent->right;

            Node* expected = nullptr;

            if (child_ptr->compare_exchange_strong(
                    expected,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);

            if (!res.curr) {
                return false;
            }

            Node* curr = res.curr;

            Node* left = get_unmarked_ref(
                curr->left.load(std::memory_order_acquire)
            );

            Node* right = get_unmarked_ref(
                curr->right.load(std::memory_order_acquire)
            );

            if (left && right) {
                Node* succ_parent = curr;
                Node* succ = right;
                bool succ_left = false;

                while (true) {
                    Node* next = get_unmarked_ref(
                        succ->left.load(std::memory_order_acquire)
                    );

                    if (!next) {
                        break;
                    }

                    succ_parent = succ;
                    succ = next;
                    succ_left = true;
                }

                int succ_val = succ->val;
                succ->val = curr->val;
                curr->val = succ_val;

                res.parent = succ_parent;
                res.curr = succ;
                res.is_left = succ_left;

                curr = succ;
            }

            std::atomic<Node*>* child_ptr =
                res.parent == nullptr
                    ? reinterpret_cast<std::atomic<Node*>*>(&root)
                    : (res.is_left ? &res.parent->left : &res.parent->right);

            Node* expected = curr;

            if (!child_ptr->compare_exchange_strong(
                    expected,
                    get_marked_ref(curr),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            Node* replacement = left ? left : right;

            Node* marked = get_marked_ref(curr);

            if (child_ptr->compare_exchange_strong(
                    marked,
                    replacement,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                delete curr;
            }

            return true;
        }
    }
};
