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
        std::atomic<bool>  marked;
        Node(int v) : val(v), left(nullptr), right(nullptr), marked(false) {}
    };

    static Node* unmark(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL);
    }
    static Node* mark_ptr(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL);
    }
    static bool is_marked(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0;
    }

    std::atomic<Node*> root;

    Node* find_parent(int key, Node*& parent) {
        parent = nullptr;
        Node* cur = unmark(root.load(std::memory_order_acquire));
        while (cur) {
            if (cur->val == key) return cur;
            parent = cur;
            cur = unmark((key < cur->val
                ? cur->left : cur->right).load(std::memory_order_acquire));
        }
        return nullptr;
    }

    void cleanup(Node* n) {
        if (!n) return;
        cleanup(unmark(n->left.load(std::memory_order_relaxed)));
        cleanup(unmark(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

public:
    ConcurrentDataStructure() {
        Node* s = new Node(INT_MIN);
        root.store(s, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() { cleanup(unmark(root.load(std::memory_order_relaxed))); }

    bool contains(int key) override {
        Node* cur = unmark(root.load(std::memory_order_acquire));
        while (cur) {
            if (cur->val == key) return !cur->marked.load(std::memory_order_acquire);
            cur = unmark((key < cur->val
                ? cur->left : cur->right).load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        Node* new_node = new Node(key);
        while (true) {
            Node* parent = nullptr;
            Node* found  = find_parent(key, parent);
            if (found) { delete new_node; return false; }
            if (!parent) {
                Node* exp = nullptr;
                if (root.compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                    return true;
                continue;
            }
            std::atomic<Node*>& slot = (key < parent->val) ? parent->left : parent->right;
            Node* exp = nullptr;
            if (slot.compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* target = find_parent(key, parent);
            if (!target || target->marked.load(std::memory_order_acquire)) return false;

            bool exp_marked = false;
            if (!target->marked.compare_exchange_strong(exp_marked, true, std::memory_order_acq_rel))
                continue;

            Node* left  = unmark(target->left.load(std::memory_order_acquire));
            Node* right = unmark(target->right.load(std::memory_order_acquire));
            Node* replacement = left ? left : right;

            if (!parent) {
                Node* exp = target;
                root.compare_exchange_strong(exp, replacement, std::memory_order_acq_rel);
            } else {
                std::atomic<Node*>& slot = (key < parent->val) ? parent->left : parent->right;
                Node* exp = target;
                slot.compare_exchange_strong(exp, replacement, std::memory_order_acq_rel);
            }
            return true;
        }
    }
};
