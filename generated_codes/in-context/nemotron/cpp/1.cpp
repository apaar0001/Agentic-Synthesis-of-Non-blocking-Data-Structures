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

        Node(int key) : val(key), left(nullptr), right(nullptr) {}
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

    struct Window {
        Node* gp;
        Node* p;
        Node* l;
    };

    bool find(int key, Window& w) {
        while (true) {
            w.gp = nullptr;
            w.p = root;
            w.l = get_unmarked_ref(w.p->left.load(std::memory_order_acquire));

            while (w.l->left.load(std::memory_order_relaxed) != nullptr) {
                w.gp = w.p;
                w.p = w.l;
                Node* l_left = w.l->left.load(std::memory_order_acquire);
                if (is_marked_ref(l_left)) {
                    if (!clean(w.gp, w.p)) {
                        goto retry;
                    }
                    goto retry;
                }
                w.l = (key < w.l->val) ? get_unmarked_ref(l_left) : get_unmarked_ref(w.l->right.load(std::memory_order_acquire));
            }

            Node* l_left = w.l->left.load(std::memory_order_acquire);
            if (is_marked_ref(l_left)) {
                if (!clean(w.p, w.l)) {
                    goto retry;
                }
                goto retry;
            }
            return true;
        retry:;
        }
    }

    bool clean(Node* p, Node* l) {
        Node* l_left = l->left.load(std::memory_order_acquire);
        if (!is_marked_ref(l_left)) return true;

        Node* p_left = p->left.load(std::memory_order_acquire);
        Node* p_right = p->right.load(std::memory_order_acquire);

        Node* sibling;
        std::atomic<Node*>* p_child_ptr;

        if (get_unmarked_ref(p_left) == l) {
            sibling = get_unmarked_ref(p_right);
            p_child_ptr = &p->left;
        } else if (get_unmarked_ref(p_right) == l) {
            sibling = get_unmarked_ref(p_left);
            p_child_ptr = &p->right;
        } else {
            return false;
        }

        Node* expected_l = l;
        Node* desired = sibling;
        return p_child_ptr->compare_exchange_strong(expected_l, desired, std::memory_order_acq_rel, std::memory_order_acquire);
    }

    void clear(Node* n) {
        if (!n) return;
        Node* u_n = get_unmarked_ref(n);
        clear(u_n->left.load(std::memory_order_relaxed));
        clear(u_n->right.load(std::memory_order_relaxed));
        delete u_n;
    }

public:
    ConcurrentDataStructure() {
        Node* left_sentinel = new Node(INT_MIN);
        Node* right_sentinel = new Node(INT_MAX);
        root = new Node(INT_MAX);

        root->left.store(left_sentinel, std::memory_order_relaxed);
        left_sentinel->left.store(nullptr, std::memory_order_relaxed);
        left_sentinel->right.store(right_sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        clear(root);
    }

    bool contains(int key) override {
        Window w;
        find(key, w);
        return w.l->val == key;
    }

    bool add(int key) override {
        while (true) {
            Window w;
            find(key, w);
            if (w.l->val == key) {
                return false;
            }

            Node* new_leaf = new Node(key);
            Node* new_internal = new Node(key < w.l->val ? w.l->val : key);

            if (key < w.l->val) {
                new_internal->left.store(new_leaf, std::memory_order_relaxed);
                new_internal->right.store(w.l, std::memory_order_relaxed);
            } else {
                new_internal->left.store(w.l, std::memory_order_relaxed);
                new_internal->right.store(new_leaf, std::memory_order_relaxed);
            }

            std::atomic<Node*>* p_child_ptr = (key < w.p->val) ? &w.p->left : &w.p->right;
            Node* expected_l = w.l;

            if (p_child_ptr->compare_exchange_strong(expected_l, new_internal, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }

            delete new_leaf;
            delete new_internal;
        }
    }

    bool remove(int key) override {
        while (true) {
            Window w;
            find(key, w);
            if (w.l->val != key) {
                return false;
            }

            Node* l_left = w.l->left.load(std::memory_order_acquire);
            if (is_marked_ref(l_left)) {
                return false;
            }

            Node* expected_null = nullptr;
            Node* marked_null = get_marked_ref(nullptr);
            if (w.l->left.compare_exchange_strong(expected_null, marked_null, std::memory_order_acq_rel, std::memory_order_acquire)) {
                if (clean(w.p, w.l)) {
                    return true;
                }
            }
        }
    }
};
