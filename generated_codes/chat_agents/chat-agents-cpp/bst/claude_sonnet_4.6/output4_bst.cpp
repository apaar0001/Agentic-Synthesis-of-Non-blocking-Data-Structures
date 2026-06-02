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
        std::atomic<bool>  deleted;
        Node(int v) : val(v), left(nullptr), right(nullptr), deleted(false) {}
    };

    static bool is_marked(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* unmark(Node* p)   { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* mark(Node* p)     { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    std::atomic<Node*> root;

    Node* find_min(Node* n) {
        while (true) {
            Node* l = unmark(n->left.load(std::memory_order_acquire));
            if (!l) return n;
            n = l;
        }
    }

    void destroy(Node* n) {
        if (!n) return;
        destroy(unmark(n->left.load(std::memory_order_relaxed)));
        destroy(unmark(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

    bool do_remove(std::atomic<Node*>& slot, int key) {
        while (true) {
            Node* cur = unmark(slot.load(std::memory_order_acquire));
            if (!cur) return false;
            if (is_marked(slot.load(std::memory_order_acquire))) return false;

            int cval = cur->val;
            if (key < cval)  return do_remove(cur->left, key);
            if (key > cval)  return do_remove(cur->right, key);

            bool exp = false;
            if (!cur->deleted.compare_exchange_strong(exp, true, std::memory_order_acq_rel))
                return false;

            Node* left  = unmark(cur->left.load(std::memory_order_acquire));
            Node* right = unmark(cur->right.load(std::memory_order_acquire));

            if (!left && !right) {
                Node* exp_ptr = cur;
                slot.compare_exchange_strong(exp_ptr, nullptr, std::memory_order_acq_rel);
                return true;
            }
            if (!left || !right) {
                Node* replacement = left ? left : right;
                Node* exp_ptr = cur;
                slot.compare_exchange_strong(exp_ptr, replacement, std::memory_order_acq_rel);
                return true;
            }

            Node* succ_parent = cur;
            Node* succ = find_min(right);
            cur->val = succ->val;
            cur->deleted.store(false, std::memory_order_release);

            bool se = false;
            succ->deleted.compare_exchange_strong(se, true, std::memory_order_acq_rel);
            Node* succ_right = unmark(succ->right.load(std::memory_order_acquire));
            Node* succ_exp = succ;
            cur->right.compare_exchange_strong(succ_exp, succ_right, std::memory_order_acq_rel);
            return true;
        }
    }

public:
    ConcurrentDataStructure() { root.store(nullptr, std::memory_order_relaxed); }
    ~ConcurrentDataStructure() { destroy(unmark(root.load(std::memory_order_relaxed))); }

    bool contains(int key) override {
        Node* cur = unmark(root.load(std::memory_order_acquire));
        while (cur) {
            if (cur->deleted.load(std::memory_order_acquire) && cur->val == key) return false;
            if (key == cur->val) return true;
            cur = unmark((key < cur->val ? cur->left : cur->right)
                         .load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        Node* new_node = new Node(key);
        while (true) {
            Node* exp = nullptr;
            if (root.compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;

            Node* cur = unmark(root.load(std::memory_order_acquire));
            Node* par = nullptr;
            std::atomic<Node*>* slot = &root;

            while (cur) {
                if (cur->val == key && !cur->deleted.load(std::memory_order_acquire)) {
                    delete new_node; return false;
                }
                par  = cur;
                slot = (key < cur->val) ? &cur->left : &cur->right;
                cur  = unmark(slot->load(std::memory_order_acquire));
            }
            Node* e = nullptr;
            if (slot->compare_exchange_strong(e, new_node, std::memory_order_acq_rel))
                return true;
        }
    }

    bool remove(int key) override { return do_remove(root, key); }
};
