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
        Node* curr;
        std::atomic<Node*>* edge;
    };

    bool cleanup_marked_child(Node* parent, std::atomic<Node*>* edge, Node* curr) {
        if (curr == nullptr) {
            return false;
        }

        Node* left = curr->left.load(std::memory_order_acquire);
        Node* right = curr->right.load(std::memory_order_acquire);

        bool left_marked = is_marked_ref(left);
        bool right_marked = is_marked_ref(right);

        if (!left_marked && !right_marked) {
            return false;
        }

        Node* replacement = nullptr;

        Node* clean_left = get_unmarked_ref(left);
        Node* clean_right = get_unmarked_ref(right);

        if (clean_left != nullptr && clean_right != nullptr) {
            return false;
        }

        replacement = clean_left != nullptr ? clean_left : clean_right;

        Node* expected = curr;
        edge->compare_exchange_strong(
            expected,
            replacement,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );

        return true;
    }

    SearchResult search(int key) {
        retry:

        Node* parent = root;
        std::atomic<Node*>* edge = &root->right;
        Node* curr = get_unmarked_ref(edge->load(std::memory_order_acquire));

        while (curr != nullptr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left) || is_marked_ref(right)) {
                if (cleanup_marked_child(parent, edge, curr)) {
                    goto retry;
                }
            }

            if (curr->val == key) {
                return { parent, curr, edge };
            }

            parent = curr;

            if (key < curr->val) {
                edge = &curr->left;
            } else {
                edge = &curr->right;
            }

            curr = get_unmarked_ref(edge->load(std::memory_order_acquire));
        }

        return { parent, nullptr, edge };
    }

    bool mark_node(Node* node) {
        while (true) {
            Node* left = node->left.load(std::memory_order_acquire);
            if (!is_marked_ref(left)) {
                if (node->left.compare_exchange_weak(
                        left,
                        get_marked_ref(left),
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return true;
                }
                continue;
            }
            return false;
        }
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        root->right.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        std::vector<Node*> stack;

        Node* first = get_unmarked_ref(root->right.load(std::memory_order_acquire));
        if (first != nullptr) {
            stack.push_back(first);
        }

        while (!stack.empty()) {
            Node* node = stack.back();
            stack.pop_back();

            Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

            if (left != nullptr) {
                stack.push_back(left);
            }

            if (right != nullptr) {
                stack.push_back(right);
            }

            delete node;
        }

        delete root;
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));

        while (curr != nullptr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            Node* right = curr->right.load(std::memory_order_acquire);

            if (!is_marked_ref(left) && !is_marked_ref(right) && curr->val == key) {
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
            SearchResult result = search(key);

            if (result.curr != nullptr) {
                Node* left = result.curr->left.load(std::memory_order_acquire);
                Node* right = result.curr->right.load(std::memory_order_acquire);

                if (!is_marked_ref(left) && !is_marked_ref(right)) {
                    return false;
                }

                continue;
            }

            Node* node = new Node(key);

            Node* expected = nullptr;
            if (result.edge->compare_exchange_strong(
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
            SearchResult result = search(key);

            if (result.curr == nullptr) {
                return false;
            }

            Node* node = result.curr;

            Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));

            if (left != nullptr && right != nullptr) {
                Node* successor_parent = node;
                Node* successor = right;
                std::atomic<Node*>* successor_edge = &node->right;

                while (true) {
                    Node* next = get_unmarked_ref(successor->left.load(std::memory_order_acquire));
                    if (next == nullptr) {
                        break;
                    }
                    successor_parent = successor;
                    successor_edge = &successor->left;
                    successor = next;
                }

                int successor_value = successor->val;

                if (!mark_node(successor)) {
                    continue;
                }

                Node* successor_right = get_unmarked_ref(
                    successor->right.load(std::memory_order_acquire)
                );

                Node* expected_successor = successor;
                successor_edge->compare_exchange_strong(
                    expected_successor,
                    successor_right,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                Node* replacement = new Node(successor_value);
                replacement->left.store(left, std::memory_order_release);
                replacement->right.store(right, std::memory_order_release);

                Node* expected_node = node;
                if (result.edge->compare_exchange_strong(
                        expected_node,
                        replacement,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    return true;
                }

                delete replacement;
                continue;
            }

            if (!mark_node(node)) {
                continue;
            }

            Node* child = left != nullptr ? left : right;

            Node* expected = node;
            if (result.edge->compare_exchange_strong(
                    expected,
                    child,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
        }
    }
};
