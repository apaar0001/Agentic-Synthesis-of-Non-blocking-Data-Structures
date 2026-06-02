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

    std::atomic<Node*> root;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    // Try to help remove a marked node by linking its parent to its non‑null child.
    // Returns true if the CAS succeeded.
    bool try_help_remove(Node* parent, bool isLeftChild, Node* marked) {
        Node* unmarked = get_unmarked_ref(marked);
        Node* replacement = nullptr;
        Node* left = get_unmarked_ref(unmarked->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(unmarked->right.load(std::memory_order_acquire));
        if (!left) replacement = right;
        else if (!right) replacement = left;
        // If both children exist we cannot safely help without more complex logic.
        if (replacement == nullptr && (left || right)) return false;

        std::atomic<Node*>& link = isLeftChild ? parent->left : parent->right;
        Node* expected = get_marked_ref(marked);
        return link.compare_exchange_strong(expected, replacement,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire);
    }

public:
    ConcurrentDataStructure() : root(nullptr) {}

    ~ConcurrentDataStructure() override {
        Node* r = get_unmarked_ref(root.load(std::memory_order_acquire));
        clear(r);
    }

    void clear(Node* node) {
        if (!node) return;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        clear(left);
        clear(right);
        delete node;
    }

    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked = get_unmarked_ref(curr);
            if (is_marked_ref(curr)) {
                // Help remove if possible, then continue using the unmarked value for direction.
                // We do not have parent here; skip helping in contains.
            }
            if (key == unmarked->val) return true;
            else if (key < unmarked->val) curr = unmarked->left.load(std::memory_order_acquire);
            else curr = unmarked->right.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* prev = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            bool dirLeft = false;
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    // Try to help remove this marked node from its parent.
                    if (!prev) {
                        // Marked root: try to replace it with a child.
                        Node* left = get_unmarked_ref(unmarked->left.load(std::memory_order_acquire));
                        Node* right = get_unmarked_ref(unmarked->right.load(std::memory_order_acquire));
                        Node* replacement = nullptr;
                        if (!left) replacement = right;
                        else if (!right) replacement = left;
                        if (replacement == nullptr && (left || right)) {
                            // Cannot help now; treat as absent and continue.
                            if (key < unmarked->val) curr = left;
                            else if (key > unmarked->val) curr = right;
                            else return false; // duplicate key (treated as absent)
                        } else {
                            Node* expected = get_marked_ref(curr);
                            if (root.compare_exchange_strong(expected, replacement,
                                                             std::memory_order_acq_rel,
                                                             std::memory_order_acquire)) {
                                // Helped succeed; restart from root.
                                break;
                            }
                            // CAS failed; retry outer loop.
                            goto outer_retry;
                        }
                    } else {
                        if (try_help_remove(prev, dirLeft, curr)) {
                            // Helped succeed; retry from same parent.
                            curr = dirLeft ? prev->left.load(std::memory_order_acquire)
                                           : prev->right.load(std::memory_order_acquire);
                            continue;
                        }
                        // Help failed; retry outer loop.
                        goto outer_retry;
                    }
                }
                if (key == unmarked->val) return false;
                prev = unmarked;
                if (key < unmarked->val) {
                    dirLeft = true;
                    curr = unmarked->left.load(std::memory_order_acquire);
                } else {
                    dirLeft = false;
                    curr = unmarked->right.load(std::memory_order_acquire);
                }
            }
            // Insertion point found (prev is parent or nullptr for empty tree)
            Node* newNode = new Node(key);
            if (!prev) {
                Node* expected = nullptr;
                if (root.compare_exchange_strong(expected, newNode,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                    return true;
                }
                // CAS failed; retry.
                continue;
            }
            std::atomic<Node*>& childPtr = dirLeft ? prev->left : prev->right;
            Node* expected = childPtr.load(std::memory_order_acquire);
            // Skip if expected is marked (should not happen because we helped marked nodes on the way)
            while (is_marked_ref(expected)) {
                // Help remove the marked child.
                if (try_help_remove(prev, dirLeft, get_unmarked_ref(expected))) {
                    expected = childPtr.load(std::memory_order_acquire);
                } else {
                    goto outer_retry;
                }
            }
            if (childPtr.compare_exchange_strong(expected, newNode,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
            // CAS failed; retry.
            outer_retry:;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* prev = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            bool dirLeft = false;
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    if (!prev) {
                        // Marked root: try to help remove it.
                        Node* left = get_unmarked_ref(unmarked->left.load(std::memory_order_acquire));
                        Node* right = get_unmarked_ref(unmarked->right.load(std::memory_order_acquire));
                        Node* replacement = nullptr;
                        if (!left) replacement = right;
                        else if (!right) replacement = left;
                        if (replacement == nullptr && (left || right)) {
                            // Cannot help; treat as absent.
                            if (key < unmarked->val) curr = left;
                            else if (key > unmarked->val) curr = right;
                            else return false;
                        } else {
                            Node* expected = get_marked_ref(curr);
                            if (root.compare_exchange_strong(expected, replacement,
                                                             std::memory_order_acq_rel,
                                                             std::memory_order_acquire)) {
                                // Node has been marked (and helped)
                                return true;
                            }
                            goto outer_retry;
                        }
                    } else {
                        if (try_help_remove(prev, dirLeft, curr)) {
                            // After helping, continue search from same parent.
                            curr = dirLeft ? prev->left.load(std::memory_order_acquire)
                                           : prev->right.load(std::memory_order_acquire);
                            continue;
                        }
                        goto outer_retry;
                    }
                }
                if (key == unmarked->val) {
                    // Attempt to logically delete this node.
                    std::atomic<Node*>& link = prev ? (dirLeft ? prev->left : prev->right) : root;
                    Node* expected = link.load(std::memory_order_acquire);
                    Node* marked = get_marked_ref(curr);
                    if (link.compare_exchange_strong(expected, marked,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                        // Node has been marked
                        return true;
                    }
                    // CAS failed; retry outer loop.
                    goto outer_retry;
                }
                prev = unmarked;
                if (key < unmarked->val) {
                    dirLeft = true;
                    curr = unmarked->left.load(std::memory_order_acquire);
                } else {
                    dirLeft = false;
                    curr = unmarked->right.load(std::memory_order_acquire);
                }
            }
            outer_retry:;
        }
    }
};