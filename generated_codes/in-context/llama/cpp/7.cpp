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

    std::atomic<Node*> root;

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
            w.p = nullptr;
            w.curr = root.load(std::memory_order_acquire);
            
            while (w.curr != nullptr) {
                Node* curr_unmarked = get_unmarked_ref(w.curr);
                if (curr_unmarked == nullptr) break;

                Node* left_val = curr_unmarked->left.load(std::memory_order_acquire);
                Node* right_val = curr_unmarked->right.load(std::memory_order_acquire);

                if (is_marked_ref(left_val) || is_marked_ref(right_val)) {
                    if (w.p == nullptr) {
                        break; 
                    }
                    Node* p_unmarked = get_unmarked_ref(w.p);
                    Node* expected = w.curr;
                    Node* desired = nullptr; 
                    
                    if (p_unmarked->left.load(std::memory_order_relaxed) == expected) {
                        if (p_unmarked->left.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                            clear_tree(curr_unmarked);
                        }
                    } else if (p_unmarked->right.load(std::memory_order_relaxed) == expected) {
                        if (p_unmarked->right.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                            clear_tree(curr_unmarked);
                        }
                    }
                    break; 
                }

                if (key == curr_unmarked->val) {
                    return true;
                }

                w.gp = w.p;
                w.p = w.curr;
                if (key < curr_unmarked->val) {
                    w.curr = left_val;
                } else {
                    w.curr = right_val;
                }
            }
            
            if (w.curr == nullptr) {
                return false;
            }
        }
    }

    void clear_tree(Node* n) {
        if (!n) return;
        Node* l = get_unmarked_ref(n->left.load(std::memory_order_relaxed));
        Node* r = get_unmarked_ref(n->right.load(std::memory_order_relaxed));
        clear_tree(l);
        clear_tree(r);
        delete n;
    }

public:
    ConcurrentDataStructure() {
        Node* inf_high = new Node(INT_MAX);
        Node* inf_low = new Node(INT_MIN);
        inf_low->right.store(inf_high, std::memory_order_relaxed);
        root.store(inf_low, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        clear_tree(get_unmarked_ref(root.load(std::memory_order_relaxed)));
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root.load(std::memory_order_acquire));
        while (curr != nullptr) {
            Node* curr_unmarked = get_unmarked_ref(curr);
            if (key == curr_unmarked->val) {
                Node* left_val = curr_unmarked->left.load(std::memory_order_acquire);
                Node* right_val = curr_unmarked->right.load(std::memory_order_acquire);
                return !is_marked_ref(left_val) && !is_marked_ref(right_val);
            }
            if (key < curr_unmarked->val) {
                curr = curr_unmarked->left.load(std::memory_order_acquire);
            } else {
                curr = curr_unmarked->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }

    bool add(int key) override {
        Node* new_node = new Node(key);
        while (true) {
            Window w;
            if (find(key, w)) {
                delete new_node;
                return false;
            }

            Node* p_unmarked = get_unmarked_ref(w.p);
            Node* expected = nullptr;
            
            if (key < p_unmarked->val) {
                if (p_unmarked->left.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                if (p_unmarked->right.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Window w;
            if (!find(key, w)) {
                return false;
            }

            Node* curr_unmarked = get_unmarked_ref(w.curr);
            Node* left_val = curr_unmarked->left.load(std::memory_order_acquire);
            if (is_marked_ref(left_val)) {
                return false;
            }

            Node* marked_left = get_marked_ref(left_val);
            if (curr_unmarked->left.compare_exchange_strong(left_val, marked_left, std::memory_order_acq_rel)) {
                Node* right_val = curr_unmarked->right.load(std::memory_order_acquire);
                Node* marked_right = get_marked_ref(right_val);
                curr_unmarked->right.compare_exchange_strong(right_val, marked_right, std::memory_order_acq_rel);

                Node* p_unmarked = get_unmarked_ref(w.p);
                Node* expected = w.curr;
                Node* desired = nullptr; 

                if (p_unmarked->left.load(std::memory_order_relaxed) == expected) {
                    if (p_unmarked->left.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                        clear_tree(curr_unmarked);
                    }
                } else if (p_unmarked->right.load(std::memory_order_relaxed) == expected) {
                    if (p_unmarked->right.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                        clear_tree(curr_unmarked);
                    }
                }
                return true;
            }
        }
    }
};
