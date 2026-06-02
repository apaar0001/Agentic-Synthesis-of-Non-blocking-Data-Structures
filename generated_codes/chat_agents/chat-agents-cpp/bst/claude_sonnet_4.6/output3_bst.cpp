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

    static uintptr_t MARK = 1UL;
    static bool is_marked(Node* p) { return (reinterpret_cast<uintptr_t>(p) & MARK) != 0; }
    static Node* unmark(Node* p)   { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~MARK); }
    static Node* mark(Node* p)     { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | MARK); }

    Node* sentinel_min;
    Node* sentinel_max;

    struct Result { Node* parent; Node* node; std::atomic<Node*>* edge; };

    Result locate(int key) {
        retry:
        Node* par  = sentinel_min;
        std::atomic<Node*>* edge = &sentinel_min->right;
        Node* cur  = unmark(edge->load(std::memory_order_acquire));

        while (cur && cur != sentinel_max) {
            if (is_marked(cur->left.load(std::memory_order_acquire)) ||
                is_marked(cur->right.load(std::memory_order_acquire))) {
                goto retry;
            }
            if (key == cur->val) return { par, cur, edge };
            par  = cur;
            edge = (key < cur->val) ? &cur->left : &cur->right;
            cur  = unmark(edge->load(std::memory_order_acquire));
        }
        return { par, nullptr, edge };
    }

    void destroy(Node* n) {
        if (!n) return;
        destroy(unmark(n->left.load(std::memory_order_relaxed)));
        destroy(unmark(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

public:
    ConcurrentDataStructure() {
        sentinel_min = new Node(INT_MIN);
        sentinel_max = new Node(INT_MAX);
        sentinel_min->right.store(sentinel_max, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() { destroy(sentinel_min); }

    bool contains(int key) override {
        auto r = locate(key);
        return r.node != nullptr;
    }

    bool add(int key) override {
        while (true) {
            auto r = locate(key);
            if (r.node) return false;
            Node* new_node = new Node(key);
            Node* exp = nullptr;
            if (r.edge->compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            auto r = locate(key);
            if (!r.node) return false;

            Node* left  = r.node->left.load(std::memory_order_acquire);
            Node* right = r.node->right.load(std::memory_order_acquire);
            if (is_marked(left) || is_marked(right)) return false;

            Node* marked_left = mark(left);
            if (!r.node->left.compare_exchange_strong(left, marked_left, std::memory_order_acq_rel))
                continue;

            Node* exp = r.node;
            Node* replacement = unmark(right) ? unmark(right) : unmark(left);
            r.edge->compare_exchange_strong(exp, unmark(replacement), std::memory_order_acq_rel);
            return true;
        }
    }
};
