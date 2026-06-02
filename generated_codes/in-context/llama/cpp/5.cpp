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

    enum State : uintptr_t { CLEAN = 0, DFLAG = 1, IFLAG = 2, MARK = 3 };

    static State get_state(Node* p) { return static_cast<State>(reinterpret_cast<uintptr_t>(p) & 3UL); }
    static Node* raw(Node* p)       { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~3UL); }
    static Node* with_state(Node* p, State s) {
        return reinterpret_cast<Node*>((reinterpret_cast<uintptr_t>(raw(p))) | static_cast<uintptr_t>(s));
    }

    Node* root;

    Node* find(int key) {
        Node* cur = raw(root->left.load(std::memory_order_acquire));
        while (cur) {
            if (key == cur->val) return cur;
            cur = raw((key < cur->val ? cur->left : cur->right).load(std::memory_order_acquire));
        }
        return nullptr;
    }

    void destroy(Node* n) {
        if (!n) return;
        destroy(raw(n->left.load(std::memory_order_relaxed)));
        destroy(raw(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        root->left.store(new Node(INT_MIN), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() { destroy(root); }

    bool contains(int key) override { return find(key) != nullptr; }

    bool add(int key) override {
        while (true) {
            Node* par = root;
            Node* cur = raw(root->left.load(std::memory_order_acquire));

            while (cur) {
                if (key == cur->val) return false;
                par = cur;
                cur = raw((key < cur->val ? cur->left : cur->right).load(std::memory_order_acquire));
            }

            Node* new_node = new Node(key);
            std::atomic<Node*>& slot = (key < par->val) ? par->left : par->right;
            Node* exp = nullptr;
            if (slot.compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* gpar = nullptr;
            Node* par  = root;
            Node* cur  = raw(root->left.load(std::memory_order_acquire));

            while (cur && cur->val != key) {
                gpar = par;
                par  = cur;
                cur  = raw((key < cur->val ? cur->left : cur->right).load(std::memory_order_acquire));
            }
            if (!cur) return false;

            Node* left  = raw(cur->left.load(std::memory_order_acquire));
            Node* right = raw(cur->right.load(std::memory_order_acquire));
            Node* replacement = left ? left : right;

            std::atomic<Node*>& slot = (key < par->val) ? par->left : par->right;
            Node* exp = cur;
            Node* flagged = with_state(cur, DFLAG);

            if (slot.compare_exchange_strong(exp, flagged, std::memory_order_acq_rel)) {
                Node* exp2 = flagged;
                slot.compare_exchange_strong(exp2, replacement, std::memory_order_acq_rel);
                return true;
            }
        }
    }
};
