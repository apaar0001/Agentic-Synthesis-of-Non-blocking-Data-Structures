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
        std::atomic<Node*>* parent_link;
    };

    bool is_logically_deleted(Node* node) {
        if (!node) return false;
        Node* l = node->left.load(std::memory_order_acquire);
        Node* r = node->right.load(std::memory_order_acquire);
        return is_marked_ref(l) || is_marked_ref(r);
    }

    bool try_cleanup(Node* parent, std::atomic<Node*>* link, Node* curr) {
        if (!parent || !link || !curr) return false;

        Node* left = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(curr->right.load(std::memory_order_acquire));

        Node* replacement = nullptr;

        if (!left) {
            replacement = right;
        } else if (!right) {
            replacement = left;
        } else {
            return false;
        }

        Node* expected = curr;
        return link->compare_exchange_strong(
            expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

    SearchResult search(int key) {
    retry:
        Node* parent = nullptr;
        Node* curr = root;
        std::atomic<Node*>* parent_link = nullptr;

        while (curr) {
            curr = get_unmarked_ref(curr);

            if (is_logically_deleted(curr)) {
                if (parent && parent_link) {
                    if (try_cleanup(parent, parent_link, curr)) {
                        curr = get_unmarked_ref(parent_link->load(std::memory_order_acquire));
                        continue;
                    } else {
                        goto retry;
                    }
                }
            }

            if (key == curr->val) {
                return {parent, curr, parent_link};
            }

            parent = curr;

            if (key < curr->val) {
                parent_link = &curr->left;
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                parent_link = &curr->right;
                curr = curr->right.load(std::memory_order_acquire);
            }
        }

        return {parent, nullptr, parent_link};
    }

    void destroy(Node* node) {
        if (!node) return;

        node = get_unmarked_ref(node);

        Node* left = get_unmarked_ref(node->left.load(std::memory_order_relaxed));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_relaxed));

        destroy(left);
        destroy(right);

        delete node;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        Node* left_sentinel = new Node(INT_MIN);
        root->left.store(left_sentinel, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = root;

        while (curr) {
            curr = get_unmarked_ref(curr);

            if (!curr) break;

            if (!is_logically_deleted(curr) && curr->val == key) {
                return true;
            }

            if (key < curr->val) {
                curr = curr->left.load(std::memory_order_acquire);
            } else {
                curr = curr->right.load(std::memory_order_acquire);
            }
        }

        return false;
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);

            if (res.curr && !is_logically_deleted(res.curr)) {
                return false;
            }

            Node* new_node = new Node(key);

            if (!res.parent) {
                delete new_node;
                continue;
            }

            std::atomic<Node*>* target_link;

            if (key < res.parent->val) {
                target_link = &res.parent->left;
            } else {
                target_link = &res.parent->right;
            }

            Node* expected = nullptr;

            if (target_link->compare_exchange_strong(
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

            if (!res.curr) {
                return false;
            }

            Node* curr = res.curr;

            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (!is_marked_ref(left)) {
                Node* marked_left = get_marked_ref(left);

                if (!curr->left.compare_exchange_strong(
                        left,
                        marked_left,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    continue;
                }
            }

            if (!is_marked_ref(right)) {
                Node* marked_right = get_marked_ref(right);

                curr->right.compare_exchange_strong(
                    right,
                    marked_right,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
            }

            if (res.parent && res.parent_link) {
                try_cleanup(res.parent, res.parent_link, curr);
            }

            return true;
        }
    }
};
