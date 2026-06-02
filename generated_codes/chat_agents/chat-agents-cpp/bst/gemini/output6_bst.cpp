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
        std::atomic<Node*>* gp_ch;
        std::atomic<Node*>* p_ch;
    };

    bool find(int key, Window& w) {
        while (true) {
            w.gp = nullptr;
            w.p = root;
            w.gp_ch = nullptr;
            w.p_ch = nullptr;

            Node* curr = root->left.load(std::memory_order_acquire);
            w.l = get_unmarked_ref(curr);
            w.p_ch = &root->left;

            while (w.l->left.load(std::memory_order_acquire) != nullptr) {
                w.gp = w.p;
                w.p = w.l;
                w.gp_ch = w.p_ch;

                if (key < w.p->val) {
                    w.p_ch = &w.p->left;
                } else {
                    w.p_ch = &w.p->right;
                }

                curr = w.p_ch->load(std::memory_order_acquire);
                if (is_marked_ref(curr)) {
                    if (!clean(w.gp, w.p, w.gp_ch, w.p_ch)) {
                        goto retry;
                    }
                    goto retry;
                }
                w.l = get_unmarked_ref(curr);
            }

            Node* l_left = w.l->left.load(std::memory_order_acquire);
            Node* l_right = w.l->right.load(std::memory_order_acquire);
            if (is_marked_ref(l_left) || is_marked_ref(l_right)) {
                clean(w.p, w.l, w.p_ch, (is_marked_ref(l_left) ? &w.l->left : &w.l->right));
                goto retry;
            }

            return true;
        retry:;
        }
    }

    bool clean(Node* parent, Node* child, std::atomic<Node*>* parent_ch, std::atomic<Node*>* child_ch) {
        Node* left_val = child->left.load(std::memory_order_acquire);
        Node* right_val = child->right.load(std::memory_order_acquire);
        Node* sibling = nullptr;

        if (is_marked_ref(left_val)) {
            sibling = get_unmarked_ref(right_val);
        } else if (is_marked_ref(right_val)) {
            sibling = get_unmarked_ref(left_val);
        } else {
            return true; 
        }

        Node* expected_child = child;
        return parent_ch->compare_exchange_strong(expected_child, sibling, std::memory_order_acq_rel);
    }

    void clear(Node* n) {
        if (!n) return;
        Node* l = get_unmarked_ref(n->left.load(std::memory_order_relaxed));
        Node* r = get_unmarked_ref(n->right.load(std::memory_order_relaxed));
        clear(l);
        clear(r);
        delete n;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        Node* left_sentinel = new Node(INT_MAX);
        left_sentinel->left.store(new Node(INT_MIN), std::memory_order_relaxed);
        left_sentinel->right.store(new Node(INT_MAX), std::memory_order_relaxed);
        root->left.store(left_sentinel, std::memory_order_relaxed);
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
        Window w;
        while (true) {
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

            Node* expected = w.l;
            if (w.p_ch->compare_exchange_strong(expected, new_internal, std::memory_order_acq_rel)) {
                return true;
            }

            delete new_leaf;
            delete new_internal;
        }
    }

    bool remove(int key) override {
        Window w;
        while (true) {
            find(key, w);
            if (w.l->val != key) {
                return false;
            }

            Node* left_ch = w.l->left.load(std::memory_order_acquire);
            Node* right_ch = w.l->right.load(std::memory_order_acquire);

            if (left_ch != nullptr || right_ch != nullptr) {
                return false;
            }

            std::atomic<Node*>* sibling_ch = nullptr;
            Node* sibling_expected = nullptr;

            if (&w.p->left == w.p_ch) {
                sibling_ch = &w.p->right;
            } else {
                sibling_ch = &w.p->left;
            }

            sibling_expected = sibling_ch->load(std::memory_order_acquire);
            if (is_marked_ref(sibling_expected)) {
                continue;
            }

            Node* expected_null = nullptr;
            if (!w.l->left.compare_exchange_strong(expected_null, get_marked_ref(nullptr), std::memory_order_acq_rel)) {
                continue;
            }

            w.l->right.store(get_marked_ref(nullptr), std::memory_order_release);

            Node* expected_l = w.l;
            if (w.p_ch->compare_exchange_strong(expected_l, get_marked_ref(w.l), std::memory_order_acq_rel)) {
                Node* expected_p = w.p;
                if (w.gp_ch->compare_exchange_strong(expected_p, sibling_expected, std::memory_order_acq_rel)) {
                    // Physical deletion succeeded
                }
            }
            return true;
        }
    }
};
