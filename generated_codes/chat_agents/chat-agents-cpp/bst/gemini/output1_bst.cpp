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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* root;

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

            while (w.curr != nullptr) {
                Node* left_raw = w.curr->left.load(std::memory_order_acquire);
                Node* right_raw = w.curr->right.load(std::memory_order_acquire);

                if (is_marked_ref(left_raw) || is_marked_ref(right_raw)) {
                    if (!clean_up(w)) {
                        break;
                    }
                    w.gp = nullptr;
                    w.p = root;
                    w.curr = get_unmarked_ref(root->left.load(std::memory_order_acquire));
                    continue;
                }

                if (key == w.curr->val) {
                    return true;
                }

                w.gp = w.p;
                w.p = w.curr;
                if (key < w.curr->val) {
                    w.curr = get_unmarked_ref(left_raw);
                } else {
                    w.curr = get_unmarked_ref(right_raw);
                }
            }
            if (w.curr == nullptr) {
                return false;
            }
        }
    }

    bool clean_up(Window& w) {
        if (w.gp == nullptr) {
            return true; 
        }

        Node* p_left_raw = w.p->left.load(std::memory_order_acquire);
        Node* p_right_raw = w.p->right.load(std::memory_order_acquire);
        bool p_left_marked = is_marked_ref(p_left_raw);
        bool p_right_marked = is_marked_ref(p_right_raw);

        if (p_left_marked || p_right_marked) {
            Node* gp_left_raw = w.gp->left.load(std::memory_order_acquire);
            Node* gp_right_raw = w.gp->right.load(std::memory_order_acquire);
            if (is_marked_ref(gp_left_raw) || is_marked_ref(gp_right_raw)) {
                return false;
            }

            Node* sibling = nullptr;
            if (p_left_marked) {
                sibling = get_unmarked_ref(p_right_raw);
            } else {
                sibling = get_unmarked_ref(p_left_raw);
            }

            std::atomic<Node*>& gp_child = (w.p == get_unmarked_ref(w.gp->left.load(std::memory_order_relaxed))) ? w.gp->left : w.gp->right;
            Node* expected = w.p;
            if (gp_child.compare_exchange_strong(expected, sibling, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            return false;
        }

        Node* curr_left_raw = w.curr->left.load(std::memory_order_acquire);
        Node* curr_right_raw = w.curr->right.load(std::memory_order_acquire);
        bool curr_left_marked = is_marked_ref(curr_left_raw);
        bool curr_right_marked = is_marked_ref(curr_right_raw);

        if (curr_left_marked && curr_right_marked) {
            std::atomic<Node*>& p_child = (w.curr == get_unmarked_ref(w.p->left.load(std::memory_order_relaxed))) ? w.p->left : w.p->right;
            Node* expected = w.curr;
            if (p_child.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            return false;
        }

        return true;
    }

    void clear(Node* n) {
        if (n == nullptr) return;
        n = get_unmarked_ref(n);
        clear(n->left.load(std::memory_order_relaxed));
        clear(n->right.load(std::memory_order_relaxed));
        delete n;
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MAX);
        root->left.store(new Node(INT_MIN), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        clear(root);
    }

    bool contains(int key) override {
        Window w;
        return find(key, w);
    }

    bool add(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        Window w;
        while (true) {
            if (find(key, w)) {
                return false;
            }

            Node* new_node = new Node(key);
            std::atomic<Node*>& child = (key < w.p->val) ? w.p->left : w.p->right;
            Node* expected = nullptr;

            if (child.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        Window w;
        while (true) {
            if (!find(key, w)) {
                return false;
            }

            Node* curr = w.curr;
            Node* left_raw = curr->left.load(std::memory_order_acquire);
            Node* right_raw = curr->right.load(std::memory_order_acquire);

            if (is_marked_ref(left_raw) || is_marked_ref(right_raw)) {
                continue;
            }

            if (left_raw == nullptr || right_raw == nullptr) {
                Node* expected_left = left_raw;
                Node* marked_left = get_marked_ref(left_raw);
                if (!curr->left.compare_exchange_strong(expected_left, marked_left, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }

                Node* expected_right = right_raw;
                Node* marked_right = get_marked_ref(right_raw);
                if (!curr->right.compare_exchange_strong(expected_right, marked_right, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }

                Node* victim = (left_raw != nullptr) ? left_raw : right_raw;
                std::atomic<Node*>& p_child = (curr == get_unmarked_ref(w.p->left.load(std::memory_order_relaxed))) ? w.p->left : w.p->right;
                Node* expected_curr = curr;
                p_child.compare_exchange_strong(expected_curr, victim, std::memory_order_acq_rel, std::memory_order_acquire);
                return true;
            } else {
                Node* succ_p = curr;
                Node* succ = get_unmarked_ref(right_raw);
                Node* succ_left = succ->left.load(std::memory_order_acquire);
                
                while (succ_left != nullptr) {
                    succ_p = succ;
                    succ = get_unmarked_ref(succ_left);
                    succ_left = succ->left.load(std::memory_order_acquire);
                }

                Node* succ_right = succ->right.load(std::memory_order_acquire);
                if (is_marked_ref(succ_right) || is_marked_ref(succ->left.load(std::memory_order_acquire))) {
                    continue;
                }

                Node* expected_s_left = nullptr;
                if (!succ->left.compare_exchange_strong(expected_s_left, get_marked_ref(nullptr), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }

                Node* expected_s_right = succ_right;
                if (!succ->right.compare_exchange_strong(expected_s_right, get_marked_ref(succ_right), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }

                int succ_val = succ->val;
                
                std::atomic<Node*>& s_p_child = (succ == get_unmarked_ref(succ_p->left.load(std::memory_order_relaxed))) ? succ_p->left : succ_p->right;
                Node* expected_succ = succ;
                s_p_child.compare_exchange_strong(expected_succ, succ_right, std::memory_order_acq_rel, std::memory_order_acquire);

                curr->val = succ_val;
                return true;
            }
        }
    }
};
