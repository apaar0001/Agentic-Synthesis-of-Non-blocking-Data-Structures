#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <vector>

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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    struct SearchResult {
        Node* parent;
        Node* node;
        std::atomic<Node*>* parent_link;
    };

    bool is_logically_deleted(Node* node) const {
        Node* l = node->left.load(std::memory_order_acquire);
        Node* r = node->right.load(std::memory_order_acquire);
        return is_marked_ref(l) && is_marked_ref(r);
    }

    bool mark_node(Node* node) {
        while (true) {
            Node* left = node->left.load(std::memory_order_acquire);
            if (!is_marked_ref(left)) {
                if (!node->left.compare_exchange_weak(
                        left,
                        get_marked_ref(left),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    continue;
                }
            }
            break;
        }

        while (true) {
            Node* right = node->right.load(std::memory_order_acquire);
            if (!is_marked_ref(right)) {
                if (!node->right.compare_exchange_weak(
                        right,
                        get_marked_ref(right),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    continue;
                }
            }
            break;
        }

        return true;
    }

    bool help_unlink(Node* parent, std::atomic<Node*>* parent_link, Node* node) {
        if (!parent_link) {
            return false;
        }

        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

        if (left && right) {
            return false;
        }

        Node* replacement = left ? left : right;

        Node* expected = node;
        return parent_link->compare_exchange_strong(
            expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire);
    }

    SearchResult search(int key) {
        retry:
        Node* parent = nullptr;
        std::atomic<Node*>* parent_link = nullptr;

        Node* curr = root;

        while (curr) {
            if (is_logically_deleted(curr)) {
                if (parent && parent_link) {
                    help_unlink(parent, parent_link, curr);
                    goto retry;
                }
            }

            if (curr->val == key) {
                return {parent, curr, parent_link};
            }

            parent = curr;

            if (key < curr->val) {
                parent_link = &curr->left;
                curr = get_unmarked_ref(
                    curr->left.load(std::memory_order_acquire));
            } else {
                parent_link = &curr->right;
                curr = get_unmarked_ref(
                    curr->right.load(std::memory_order_acquire));
            }
        }

        return {parent, nullptr, parent_link};
    }

    void destroy(Node* node) {
        if (!node) {
            return;
        }

        std::vector<Node*> stack;
        stack.push_back(node);

        while (!stack.empty()) {
            Node* curr = stack.back();
            stack.pop_back();

            Node* left =
                get_unmarked_ref(curr->left.load(std::memory_order_relaxed));
            Node* right =
                get_unmarked_ref(curr->right.load(std::memory_order_relaxed));

            if (left) {
                stack.push_back(left);
            }

            if (right) {
                stack.push_back(right);
            }

            delete curr;
        }
    }

public:
    ConcurrentDataStructure() {
        Node* min_node = new Node(INT_MIN);
        Node* max_node = new Node(INT_MAX);
        min_node->right.store(max_node, std::memory_order_release);
        root = min_node;
    }

    ~ConcurrentDataStructure() override {
        destroy(root);
    }

    bool contains(int key) override {
        Node* curr = root;

        while (curr) {
            if (!is_logically_deleted(curr) && curr->val == key) {
                return true;
            }

            if (key < curr->val) {
                curr = get_unmarked_ref(
                    curr->left.load(std::memory_order_acquire));
            } else {
                curr = get_unmarked_ref(
                    curr->right.load(std::memory_order_acquire));
            }
        }

        return false;
    }

    bool add(int key) override {
        if (key == INT_MIN || key == INT_MAX) {
            return false;
        }

        while (true) {
            SearchResult s = search(key);

            if (s.node && !is_logically_deleted(s.node)) {
                return false;
            }

            Node* parent = s.parent;
            std::atomic<Node*>* link = s.parent_link;

            if (!parent || !link) {
                continue;
            }

            Node* new_node = new Node(key);

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
        if (key == INT_MIN || key == INT_MAX) {
            return false;
        }

        while (true) {
            SearchResult s = search(key);

            if (!s.node) {
                return false;
            }

            Node* node = s.node;

            Node* left =
                get_unmarked_ref(node->left.load(std::memory_order_acquire));
            Node* right =
                get_unmarked_ref(node->right.load(std::memory_order_acquire));

            if (left && right) {
                Node* succ_parent = node;
                std::atomic<Node*>* succ_link = &node->right;
                Node* succ = right;

                while (true) {
                    Node* next =
                        get_unmarked_ref(succ->left.load(std::memory_order_acquire));

                    if (!next) {
                        break;
                    }

                    succ_parent = succ;
                    succ_link = &succ->left;
                    succ = next;
                }

                int succ_val = succ->val;

                if (!remove(succ_val)) {
                    continue;
                }

                node->val = succ_val;
                return true;
            }

            mark_node(node);

            if (help_unlink(s.parent, s.parent_link, node)) {
                return true;
            }

            if (!s.parent) {
                return false;
            }
        }
    }
};
