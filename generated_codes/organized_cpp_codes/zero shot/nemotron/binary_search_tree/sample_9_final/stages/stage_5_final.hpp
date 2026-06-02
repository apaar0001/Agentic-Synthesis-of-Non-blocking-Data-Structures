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
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1LU) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1LU);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1LU);
    }

    Node* head; // sentinel with INT_MIN
    Node* sentinel_max; // sentinel with INT_MAX

    // Helper to find node and its parent for a given key.
    // Returns pair (parent, node) where node is the first node with val == key
    // or the leaf where it should be inserted. parent may be nullptr for head.
    std::pair<Node*, Node*> find(int key) const {
        Node* parent = nullptr;
        Node* curr = head;
        while (true) {
            Node* left = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(curr->right.load(std::memory_order_acquire));
            if (curr->val == key) {
                return {parent, curr};
            }
            Node* next = (key < curr->val) ? left : right;
            if (next == nullptr) {
                return {parent, curr};
            }
            parent = curr;
            curr = next;
        }
    }

    // Helper to physically remove a node after it has been logically marked.
    // Assumes parent->child pointer points to a marked node.
    void help_remove(Node* parent, bool is_left_child, Node* marked_node) {
        Node* unmarked = get_unmarked_ref(marked_node);
        Node* replacement = nullptr;
        Node* left = get_unmarked_ref(unmarked->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(unmarked->right.load(std::memory_order_acquire));
        if (left != nullptr) {
            replacement = left;
            // Find rightmost node in left subtree
            while (true) {
                Node* r = get_unmarked_ref(replacement->right.load(std::memory_order_acquire));
                if (r == nullptr) break;
                replacement = r;
            }
            // Copy replacement's value into the node to be removed (lazy approach)
            unmarked->val = replacement->val;
            // Now we need to remove the replacement node (which has at most left child)
            // Find parent of replacement
            Node* rep_parent = unmarked;
            Node* rep_cur = left;
            while (rep_cur != replacement) {
                Node* l = get_unmarked_ref(rep_cur->left.load(std::memory_order_acquire));
                Node* r = get_unmarked_ref(rep_cur->right.load(std::memory_order_acquire));
                if (replacement->val < rep_cur->val) {
                    rep_parent = rep_cur;
                    rep_cur = l;
                } else {
                    rep_parent = rep_cur;
                    rep_cur = r;
                }
            }
            // Now replacement is rep_cur, and we can splice it out
            Node* rep_left = get_unmarked_ref(replacement->left.load(std::memory_order_acquire));
            if (rep_parent->left.load(std::memory_order_acquire) == replacement) {
                rep_parent->left.compare_exchange_strong(replacement, rep_left,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            } else {
                rep_parent->right.compare_exchange_strong(replacement, rep_left,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            delete replacement;
        } else if (right != nullptr) {
            replacement = right;
            // Find leftmost node in right subtree
            while (true) {
                Node* l = get_unmarked_ref(replacement->left.load(std::memory_order_acquire));
                if (l == nullptr) break;
                replacement = l;
            }
            unmarked->val = replacement->val;
            Node* rep_parent = unmarked;
            Node* rep_cur = right;
            while (rep_cur != replacement) {
                Node* l = get_unmarked_ref(rep_cur->left.load(std::memory_order_acquire));
                Node* r = get_unmarked_ref(rep_cur->right.load(std::memory_order_acquire));
                if (replacement->val < rep_cur->val) {
                    rep_parent = rep_cur;
                    rep_cur = l;
                } else {
                    rep_parent = rep_cur;
                    rep_cur = r;
                }
            }
            Node* rep_right = get_unmarked_ref(replacement->right.load(std::memory_order_acquire));
            if (rep_parent->left.load(std::memory_order_acquire) == replacement) {
                rep_parent->left.compare_exchange_strong(replacement, rep_right,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            } else {
                rep_parent->right.compare_exchange_strong(replacement, rep_right,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            delete replacement;
        } else {
            // leaf node: just unlink
            Node* expected = marked_node;
            Node* desired = nullptr;
            if (is_left_child) {
                parent->left.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            } else {
                parent->right.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            delete unmarked;
        }
    }

    // Recursive destructor helper
    void destroy(Node* node) {
        if (node == nullptr) return;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        destroy(left);
        destroy(right);
        delete node;
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        sentinel_max = new Node(INT_MAX);
        head->right.store(sentinel_max, std::memory_order_release);
        sentinel_max->left.store(head, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        // Break the circular sentinel links to avoid double delete
        head->right.store(nullptr, std::memory_order_release);
        sentinel_max->left.store(nullptr, std::memory_order_release);
        destroy(head);
        destroy(sentinel_max);
    }

    bool contains(int key) override {
        Node* curr = head;
        while (true) {
            Node* left = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(curr->right.load(std::memory_order_acquire));
            if (curr->val == key) {
                // If the node is marked, treat as absent
                return !is_marked_ref(curr);
            }
            Node* next = (key < curr->val) ? left : right;
            if (next == nullptr) {
                return false;
            }
            curr = next;
        }
    }

    bool add(int key) override {
        while (true) {
            auto [parent, node] = find(key);
            if (node->val == key) {
                // key already present (whether marked or not)
                return false;
            }
            Node* newNode = new Node(key);
            newNode->left.store(nullptr, std::memory_order_relaxed);
            newNode->right.store(nullptr, std::memory_order_relaxed);
            bool left_insert = (key < parent->val);
            Node* expected = left_insert ? parent->left.load(std::memory_order_acquire)
                                         : parent->right.load(std::memory_order_acquire);
            // Help remove any marked node that might be in the expected slot
            if (is_marked_ref(expected)) {
                Node* marked = expected;
                Node* unmarked = get_unmarked_ref(marked);
                help_remove(parent, left_insert, marked);
                continue; // retry insertion
            }
            Node* desired = newNode;
            if (left_insert) {
                if (parent->left.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            } else {
                if (parent->right.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
            }
            // CAS failed, retry
        }
    }

    bool remove(int key) override {
        while (true) {
            auto [parent, node] = find(key);
            if (node->val != key) {
                return false; // not present
            }
            // Try to logically mark the node via its parent pointer
            bool is_left = (parent != nullptr && key < parent->val);
            Node* expected = (parent != nullptr) ?
                (is_left ? parent->left.load(std::memory_order_acquire)
                         : parent->right.load(std::memory_order_acquire))
                : nullptr;
            // If parent is nullptr (should not happen because head is sentinel)
            if (parent == nullptr) {
                return false;
            }
            Node* marked = get_marked_ref(node);
            if (is_left) {
                if (parent->left.compare_exchange_strong(expected, marked,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Successfully marked, now help physically remove
                    help_remove(parent, true, marked);
                    return true;
                }
            } else {
                if (parent->right.compare_exchange_strong(expected, marked,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    help_remove(parent, false, marked);
                    return true;
                }
            }
            // CAS failed, retry
        }
    }
};