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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static bool cas_child(std::atomic<Node*>& child, Node*& expected, Node* desired) {
        return child.compare_exchange_strong(
            expected,
            desired,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    bool cleanup_marked(Node* parent, bool is_left, Node* node) {
        if (!parent || !node) {
            return false;
        }

        std::atomic<Node*>& ref = is_left ? parent->left : parent->right;

        Node* current = ref.load(std::memory_order_acquire);
        if (get_unmarked_ref(current) != node) {
            return false;
        }

        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

        if (left && right) {
            return false;
        }

        Node* replacement = left ? left : right;
        Node* expected = current;

        return ref.compare_exchange_strong(
            expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    bool find(int key, Node*& parent, Node*& curr, bool& is_left_child) {
    retry:
        parent = root;
        curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));
        is_left_child = false;

        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left) || is_marked_ref(right)) {
                if (!cleanup_marked(parent, is_left_child, curr)) {
                    goto retry;
                }
                goto retry;
            }

            if (key == curr->val) {
                return true;
            }

            parent = curr;

            if (key < curr->val) {
                curr = get_unmarked_ref(left);
                is_left_child = true;
            } else {
                curr = get_unmarked_ref(right);
                is_left_child = false;
            }
        }

        return false;
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
        Node* tail = new Node(INT_MAX);
        root->right.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));

        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left) || is_marked_ref(right)) {
                return false;
            }

            if (curr->val == key) {
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
            Node* parent;
            Node* curr;
            bool is_left_child;

            if (find(key, parent, curr, is_left_child)) {
                return false;
            }

            Node* node = new Node(key);

            std::atomic<Node*>& target =
                is_left_child ? parent->left : parent->right;

            Node* expected = nullptr;

            if (target.compare_exchange_strong(
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
            Node* parent;
            Node* curr;
            bool is_left_child;

            if (!find(key, parent, curr, is_left_child)) {
                return false;
            }

            Node* left = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(curr->right.load(std::memory_order_acquire));

            if (left && right) {
                Node* succ_parent = curr;
                Node* succ = right;
                bool succ_is_left = false;

                while (true) {
                    Node* next = get_unmarked_ref(
                        succ->left.load(std::memory_order_acquire)
                    );

                    if (!next) {
                        break;
                    }

                    succ_parent = succ;
                    succ = next;
                    succ_is_left = true;
                }

                int succ_val = succ->val;

                if (!remove(succ_val)) {
                    continue;
                }

                curr->val = succ_val;
                return true;
            }

            if (!left && !right) {
                Node* expected_left = nullptr;
                Node* marked_left = get_marked_ref(nullptr);

                curr->left.compare_exchange_strong(
                    expected_left,
                    marked_left,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                Node* expected_right = nullptr;
                Node* marked_right = get_marked_ref(nullptr);

                curr->right.compare_exchange_strong(
                    expected_right,
                    marked_right,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
            } else if (left) {
                Node* expected = left;

                curr->left.compare_exchange_strong(
                    expected,
                    get_marked_ref(left),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
            } else {
                Node* expected = right;

                curr->right.compare_exchange_strong(
                    expected,
                    get_marked_ref(right),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
            }

            if (cleanup_marked(parent, is_left_child, curr)) {
                return true;
            }
        }
    }
};
