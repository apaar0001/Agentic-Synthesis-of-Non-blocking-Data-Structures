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
        Node* curr;
        bool is_left;
    };

    bool try_cleanup(Node* parent, Node* curr, bool is_left) {
        if (!parent || !curr) {
            return false;
        }

        Node* left = curr->left.load(std::memory_order_acquire);
        Node* right = curr->right.load(std::memory_order_acquire);

        bool left_marked = is_marked_ref(left);
        bool right_marked = is_marked_ref(right);

        if (!left_marked && !right_marked) {
            return false;
        }

        Node* child = nullptr;

        Node* ul = get_unmarked_ref(left);
        Node* ur = get_unmarked_ref(right);

        if (ul && ur) {
            return false;
        }

        child = ul ? ul : ur;

        std::atomic<Node*>* edge =
            is_left ? &parent->left : &parent->right;

        Node* expected = curr;

        return edge->compare_exchange_strong(
            expected,
            child,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    SearchResult search(int key) {
    retry:
        Node* parent = root;
        Node* curr = get_unmarked_ref(
            root->right.load(std::memory_order_acquire)
        );
        bool is_left = false;

        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left) || is_marked_ref(right)) {
                if (try_cleanup(parent, curr, is_left)) {
                    curr = get_unmarked_ref(
                        is_left
                            ? parent->left.load(std::memory_order_acquire)
                            : parent->right.load(std::memory_order_acquire)
                    );
                    continue;
                }
                goto retry;
            }

            if (curr->val == key) {
                return {parent, curr, is_left};
            }

            parent = curr;

            if (key < curr->val) {
                curr = get_unmarked_ref(
                    curr->left.load(std::memory_order_acquire)
                );
                is_left = true;
            } else {
                curr = get_unmarked_ref(
                    curr->right.load(std::memory_order_acquire)
                );
                is_left = false;
            }
        }

        return {parent, nullptr, is_left};
    }

    void destroy(Node* node) {
        if (!node) {
            return;
        }

        Node* left = get_unmarked_ref(
            node->left.load(std::memory_order_relaxed)
        );

        Node* right = get_unmarked_ref(
            node->right.load(std::memory_order_relaxed)
        );

        destroy(left);
        destroy(right);

        delete node;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MIN);

        Node* max_node = new Node(INT_MAX);

        root->right.store(
            max_node,
            std::memory_order_release
        );
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(
            root->right.load(std::memory_order_acquire)
        );

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
            SearchResult res = search(key);

            if (res.curr && res.curr->val == key) {
                return false;
            }

            Node* parent = res.parent;

            Node* node = new Node(key);

            std::atomic<Node*>* edge =
                res.is_left ? &parent->left : &parent->right;

            Node* expected = nullptr;

            if (edge->compare_exchange_strong(
                    expected,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                )) {
                return true;
            }

            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);

            Node* curr = res.curr;

            if (!curr) {
                return false;
            }

            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            Node* ul = get_unmarked_ref(left);
            Node* ur = get_unmarked_ref(right);

            if (ul && ur) {
                Node* succ_parent = curr;
                Node* succ = ur;
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

            if (!ul) {
                Node* expected = right;

                if (!is_marked_ref(expected) &&
                    curr->right.compare_exchange_strong(
                        expected,
                        get_marked_ref(expected),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    )) {

                    try_cleanup(res.parent, curr, res.is_left);
                    return true;
                }
            } else {
                Node* expected = left;

                if (!is_marked_ref(expected) &&
                    curr->left.compare_exchange_strong(
                        expected,
                        get_marked_ref(expected),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    )) {

                    try_cleanup(res.parent, curr, res.is_left);
                    return true;
                }
            }
        }
    }
};
