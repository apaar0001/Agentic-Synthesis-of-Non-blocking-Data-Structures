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
        Node* curr;
    };

    bool find(int key, Window& w) {
        while (true) {
            w.gp = nullptr;
            w.p = root;
            w.curr = get_unmarked_ref(root->left.load(std::memory_order_acquire));

            while (true) {
                if (w.curr == nullptr) return false;

                Node* left_val = w.curr->left.load(std::memory_order_acquire);
                Node* right_val = w.curr->right.load(std::memory_order_acquire);

                if (is_marked_ref(left_val) || is_marked_ref(right_val)) {
                    if (!clean_node(w.gp, w.p, w.curr)) {
                        break; 
                    }
                    w.curr = get_unmarked_ref(w.p->left.load(std::memory_order_acquire));
                    if (w.curr != nullptr && key > w.p->val) {
                        w.curr = get_unmarked_ref(w.p->right.load(std::memory_order_acquire));
                    }
                    continue;
                }

                int c_val = w.curr->val;
                if (key == c_val) return true;

                w.gp = w.p;
                w.p = w.curr;
                if (key < c_val) {
                    w.curr = get_unmarked_ref(left_val);
                } else {
                    w.curr = get_unmarked_ref(right_val);
                }
            }
        }
    }

    bool clean_node(Node* gp, Node* p, Node* curr) {
        Node* left_val = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
        Node* right_val = get_unmarked_ref(curr->right.load(std::memory_order_acquire));
        Node* replace = (left_val != nullptr) ? left_val : right_val;

        if (gp == nullptr) return false;

        std::atomic<Node*>& p_child = (curr == get_unmarked_ref(p->left.load(std::memory_order_relaxed))) ? p->left : p->right;
        Node* expected_curr = curr;
        if (p_child.compare_exchange_strong(expected_curr, replace, std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
        return false;
    }

    void delete_tree(Node* n) {
        if (!n) return;
        Node* l = get_unmarked_ref(n->left.load(std::memory_order_relaxed));
        Node* r = get_unmarked_ref(n->right.load(std::memory_order_relaxed));
        delete_tree(l);
        delete_tree(r);
        delete n;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        root->left.store(new Node(INT_MIN), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        delete_tree(root);
    }

    bool contains(int key) override {
        Window w;
        return find(key, w);
    }

    bool add(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        Window w;
        while (true) {
            if (find(key, w)) return false;

            Node* new_node = new Node(key);
            std::atomic<Node*>& p_child = (key < w.p->val) ? w.p->left : w.p->right;
            Node* expected = nullptr;

            if (p_child.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        Window w;
        while (true) {
            if (!find(key, w)) return false;

            Node* curr = w.curr;
            Node* left_val = curr->left.load(std::memory_order_acquire);
            Node* right_val = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left_val) || is_marked_ref(right_val)) continue;

            if (left_val != nullptr && right_val != nullptr) {
                Node* succ = get_unmarked_ref(right_val);
                Node* succ_left = succ->left.load(std::memory_order_acquire);
                while (succ_left != nullptr) {
                    succ = get_unmarked_ref(succ_left);
                    succ_left = succ->left.load(std::memory_order_acquire);
                }
                int succ_val = succ->val;
                if (remove(succ_val)) {
                    curr->val = succ_val;
                    return true;
                }
                continue;
            }

            std::atomic<Node*>& target_atom = (left_val != nullptr) ? curr->right : curr->left;
            Node* expected_null = nullptr;
            Node* marked_null = get_marked_ref(nullptr);
            if (!target_atom.compare_exchange_strong(expected_null, marked_null, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }

            std::atomic<Node*>& mark_atom = (left_val != nullptr) ? curr->left : curr->right;
            Node* expected_child = get_unmarked_ref(mark_atom.load(std::memory_order_acquire));
            Node* marked_child = get_marked_ref(expected_child);
            if (mark_atom.compare_exchange_strong(expected_child, marked_child, std::memory_order_acq_rel, std::memory_order_acquire)) {
                clean_node(w.gp, w.p, curr);
                return true;
            }
        }
    }
};
