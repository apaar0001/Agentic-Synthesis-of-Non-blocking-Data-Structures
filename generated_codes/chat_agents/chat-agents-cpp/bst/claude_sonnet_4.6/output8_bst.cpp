#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static bool is_marked(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* unmark(Node* p)   { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* mark(Node* p)     { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    Node* root;

    struct SeekRecord {
        Node* ancestor;
        Node* successor;
        Node* parent;
        Node* leaf;
    };

    void seek(int key, SeekRecord& rec) {
        Node* ancestor  = root;
        Node* successor = unmark(root->left.load(std::memory_order_acquire));
        Node* parent    = successor;
        Node* leaf      = unmark(parent ? parent->left.load(std::memory_order_acquire) : nullptr);

        while (leaf != nullptr) {
            if (!is_marked(parent->left.load(std::memory_order_acquire)) &&
                !is_marked(parent->right.load(std::memory_order_acquire))) {
                ancestor  = parent;
                successor = leaf;
            }
            parent = leaf;
            if (key < leaf->val)
                leaf = unmark(leaf->left.load(std::memory_order_acquire));
            else if (key > leaf->val)
                leaf = unmark(leaf->right.load(std::memory_order_acquire));
            else
                break;
        }
        rec = { ancestor, successor, parent, leaf };
    }

    std::atomic<Node*>& child_ref(Node* n, int key) {
        return (key < n->val) ? n->left : n->right;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        Node* left_sentinel = new Node(INT_MIN);
        root->left.store(left_sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        std::function<void(Node*)> del = [&](Node* n) {
            if (!n) return;
            del(unmark(n->left.load(std::memory_order_relaxed)));
            del(unmark(n->right.load(std::memory_order_relaxed)));
            delete n;
        };
        del(root);
    }

    bool contains(int key) override {
        Node* cur = unmark(root->left.load(std::memory_order_acquire));
        while (cur) {
            if (key == cur->val) return !is_marked(cur->left.load(std::memory_order_acquire));
            cur = unmark((key < cur->val
                ? cur->left : cur->right).load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            SeekRecord rec;
            seek(key, rec);
            if (rec.leaf && rec.leaf->val == key) return false;

            Node* new_leaf   = new Node(key);
            Node* new_internal = new Node(rec.leaf ? std::max(key, rec.leaf->val) : key);
            if (!rec.leaf || key < rec.leaf->val) {
                new_internal->left.store(new_leaf,   std::memory_order_relaxed);
                new_internal->right.store(rec.leaf,  std::memory_order_relaxed);
            } else {
                new_internal->left.store(rec.leaf,   std::memory_order_relaxed);
                new_internal->right.store(new_leaf,  std::memory_order_relaxed);
            }
            Node* expected = rec.leaf;
            if (child_ref(rec.parent, key).compare_exchange_strong(
                    expected, new_internal, std::memory_order_acq_rel))
                return true;
            delete new_leaf;
            delete new_internal;
        }
    }

    bool remove(int key) override {
        while (true) {
            SeekRecord rec;
            seek(key, rec);
            if (!rec.leaf || rec.leaf->val != key) return false;

            std::atomic<Node*>& target_ref = child_ref(rec.parent, key);
            Node* cur = rec.leaf;
            Node* marked = mark(cur);
            if (!target_ref.compare_exchange_strong(cur, marked, std::memory_order_acq_rel))
                continue;

            Node* sibling = (rec.parent->left.load(std::memory_order_acquire) == marked)
                ? unmark(rec.parent->right.load(std::memory_order_acquire))
                : unmark(rec.parent->left.load(std::memory_order_acquire));

            Node* exp = rec.successor;
            child_ref(rec.ancestor, key).compare_exchange_strong(
                exp, sibling, std::memory_order_acq_rel);
            return true;
        }
    }
};
