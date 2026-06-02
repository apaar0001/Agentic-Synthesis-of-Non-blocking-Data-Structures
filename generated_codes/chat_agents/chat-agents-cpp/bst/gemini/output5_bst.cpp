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

    void delete_tree(Node* n) {
        if (!n) return;
        Node* curr = get_unmarked_ref(n);
        delete_tree(curr->left.load(std::memory_order_relaxed));
        delete_tree(curr->right.load(std::memory_order_relaxed));
        delete curr;
    }

    bool find_position(int key, Node*& parent, Node*& curr, std::atomic<Node*>*& parent_ptr) {
        while (true) {
            parent = root;
            parent_ptr = &root->right;
            curr = get_unmarked_ref(parent->right.load(std::memory_order_acquire));

            while (curr != nullptr) {
                Node* left_val = curr->left.load(std::memory_order_acquire);
                Node* right_val = curr->right.load(std::memory_order_acquire);

                if (is_marked_ref(left_val) || is_marked_ref(right_val)) {
                    Node* unmarked_left = get_unmarked_ref(left_val);
                    Node* unmarked_right = get_unmarked_ref(right_val);
                    Node* expected = curr;
                    
                    if (unmarked_left == nullptr && unmarked_right == nullptr) {
                        if (parent_ptr->compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel)) {
                            delete curr;
                        }
                    } else if (unmarked_left != nullptr && unmarked_right == nullptr) {
                        if (parent_ptr->compare_exchange_strong(expected, unmarked_left, std::memory_order_acq_rel)) {
                            delete curr;
                        }
                    } else if (unmarked_left == nullptr && unmarked_right != nullptr) {
                        if (parent_ptr->compare_exchange_strong(expected, unmarked_right, std::memory_order_acq_rel)) {
                            delete curr;
                        }
                    } else {
                        Node* succ_parent = curr;
                        std::atomic<Node*>* succ_parent_ptr = &curr->right;
                        Node* succ = get_unmarked_ref(right_val);
                        while (true) {
                            Node* succ_left = succ->left.load(std::memory_order_acquire);
                            if (succ_left == nullptr) break;
                            succ_parent = succ;
                            succ_parent_ptr = &succ->left;
                            succ = get_unmarked_ref(succ_left);
                        }
                        int succ_val = succ->val;
                        if (remove(succ_val)) {
                            curr->val = succ_val;
                        }
                    }
                    goto retry;
                }

                if (key == curr->val) {
                    return true;
                } else if (key < curr->val) {
                    parent = curr;
                    parent_ptr = &curr->left;
                    curr = get_unmarked_ref(left_val);
                } else {
                    parent = curr;
                    parent_ptr = &curr->right;
                    curr = get_unmarked_ref(right_val);
                }
            }
            return false;
        retry:;
        }
    }

public:
    ConcurrentDataStructure() {
        root = new Node(INT_MIN);
        root->right.store(new Node(INT_MAX), std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        delete_tree(root);
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root->right.load(std::memory_order_acquire));
        while (curr != nullptr) {
            Node* left_val = curr->left.load(std::memory_order_acquire);
            Node* right_val = curr->right.load(std::memory_order_acquire);
            
            if (is_marked_ref(left_val) || is_marked_ref(right_val)) {
                Node* parent;
                std::atomic<Node*>* parent_ptr;
                find_position(key, parent, curr, parent_ptr);
                return contains(key);
            }

            if (key == curr->val) {
                return true;
            } else if (key < curr->val) {
                curr = get_unmarked_ref(left_val);
            } else {
                curr = get_unmarked_ref(right_val);
            }
        }
        return false;
    }

    bool add(int key) override {
        Node* parent;
        Node* curr;
        std::atomic<Node*>* parent_ptr;
        while (true) {
            if (find_position(key, parent, curr, parent_ptr)) {
                return false;
            }
            Node* new_node = new Node(key);
            Node* expected = nullptr;
            if (parent_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        Node* parent;
        Node* curr;
        std::atomic<Node*>* parent_ptr;
        while (true) {
            if (!find_position(key, parent, curr, parent_ptr)) {
                return false;
            }

            Node* left_val = curr->left.load(std::memory_order_acquire);
            if (!is_marked_ref(left_val)) {
                if (curr->left.compare_exchange_strong(left_val, get_marked_ref(left_val), std::memory_order_acq_rel)) {
                    Node* right_val = curr->right.load(std::memory_order_acquire);
                    if (!is_marked_ref(right_val)) {
                        curr->right.compare_exchange_strong(right_val, get_marked_ref(right_val), std::memory_order_acq_rel);
                    }
                    find_position(key, parent, curr, parent_ptr);
                    return true;
                }
            }
        }
    }
};
