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
        
        explicit Node(int key) : val(key), left(nullptr), right(nullptr) {}
    };
    
    std::atomic<Node*> root;
    
    static bool is_marked_ref(Node* ptr) {
        return ptr && (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        if (!ptr) return nullptr;
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        if (!ptr) return nullptr;
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    Node* find_min(Node* node) const {
        Node* curr = get_unmarked_ref(node);
        while (curr) {
            Node* left = curr->left.load(std::memory_order_acquire);
            if (!left) break;
            curr = get_unmarked_ref(left);
        }
        return curr;
    }
    
    bool try_mark_left(Node* node, Node* expected) {
        Node* marked = get_marked_ref(expected);
        return node->left.compare_exchange_strong(expected, marked, std::memory_order_acq_rel);
    }
    
    bool try_mark_right(Node* node, Node* expected) {
        Node* marked = get_marked_ref(expected);
        return node->right.compare_exchange_strong(expected, marked, std::memory_order_acq_rel);
    }
    
    void help_remove(Node* parent, Node* child, bool is_left) {
        Node* unmarked_child = get_unmarked_ref(child);
        Node* child_left = unmarked_child->left.load(std::memory_order_acquire);
        Node* child_right = unmarked_child->right.load(std::memory_order_acquire);
        
        Node* next = nullptr;
        if (child_left && !child_right) {
            next = child_left;
        } else if (!child_left && child_right) {
            next = child_right;
        } else if (child_left && child_right) {
            next = child_right;
        }
        
        if (is_left) {
            parent->left.compare_exchange_strong(child, next, std::memory_order_acq_rel);
        } else {
            parent->right.compare_exchange_strong(child, next, std::memory_order_acq_rel);
        }
    }
    
    void cleanup_marked_nodes(Node* node) {
        if (!node) return;
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        
        if (is_marked_ref(left)) {
            Node* unmarked_left = get_unmarked_ref(left);
            Node* left_left = unmarked_left->left.load(std::memory_order_acquire);
            Node* left_right = unmarked_left->right.load(std::memory_order_acquire);
            Node* next = nullptr;
            if (left_left && !left_right) {
                next = left_left;
            } else if (!left_left && left_right) {
                next = left_right;
            } else if (left_left && left_right) {
                next = left_right;
            }
            node->left.compare_exchange_strong(left, next, std::memory_order_acq_rel);
        }
        
        if (is_marked_ref(right)) {
            Node* unmarked_right = get_unmarked_ref(right);
            Node* right_left = unmarked_right->left.load(std::memory_order_acquire);
            Node* right_right = unmarked_right->right.load(std::memory_order_acquire);
            Node* next = nullptr;
            if (right_left && !right_right) {
                next = right_left;
            } else if (!right_left && right_right) {
                next = right_right;
            } else if (right_left && right_right) {
                next = right_right;
            }
            node->right.compare_exchange_strong(right, next, std::memory_order_acq_rel);
        }
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_relaxed);
        if (r) {
            delete get_unmarked_ref(r);
        }
    }
    
    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked = get_unmarked_ref(curr);
            if (!unmarked) break;
            
            if (unmarked->val == key) {
                return !is_marked_ref(curr);
            }
            
            cleanup_marked_nodes(unmarked);
            
            if (key < unmarked->val) {
                curr = unmarked->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            std::atomic<Node*>* child_ptr = &root;
            
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (!unmarked) break;
                
                if (unmarked->val == key) {
                    if (is_marked_ref(curr)) {
                        if (parent) {
                            help_remove(parent, curr, child_ptr == &parent->left);
                        }
                        return add(key);
                    }
                    return false;
                }
                
                parent = unmarked;
                if (key < unmarked->val) {
                    child_ptr = &unmarked->left;
                    curr = unmarked->left.load(std::memory_order_acquire);
                } else {
                    child_ptr = &unmarked->right;
                    curr = unmarked->right.load(std::memory_order_acquire);
                }
                
                if (curr && is_marked_ref(curr)) {
                    if (parent) {
                        help_remove(parent, curr, child_ptr == &parent->left);
                    }
                    return add(key);
                }
            }
            
            Node* new_node = new Node(key);
            Node* expected = nullptr;
            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            std::atomic<Node*>* child_ptr = &root;
            
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (!unmarked) break;
                
                if (unmarked->val == key) {
                    if (is_marked_ref(curr)) {
                        return false;
                    }
                    
                    Node* left = unmarked->left.load(std::memory_order_acquire);
                    Node* right = unmarked->right.load(std::memory_order_acquire);
                    
                    if (!left) {
                        Node* marked_right = get_marked_ref(right);
                        if (unmarked->right.compare_exchange_strong(right, marked_right, std::memory_order_acq_rel)) {
                            // Node has been marked
                            Node* expected = curr;
                            child_ptr->compare_exchange_strong(expected, right, std::memory_order_acq_rel);
                            return true;
                        }
                        continue;
                    } else if (!right) {
                        Node* marked_left = get_marked_ref(left);
                        if (unmarked->left.compare_exchange_strong(left, marked_left, std::memory_order_acq_rel)) {
                            // Node has been marked
                            Node* expected = curr;
                            child_ptr->compare_exchange_strong(expected, left, std::memory_order_acq_rel);
                            return true;
                        }
                        continue;
                    } else {
                        Node* successor = find_min(right);
                        if (!successor) continue;
                        
                        int succ_val = successor->val;
                        bool removed = remove(succ_val);
                        if (removed) {
                            Node* new_node = new Node(succ_val);
                            new_node->left.store(left, std::memory_order_release);
                            new_node->right.store(right, std::memory_order_release);
                            
                            Node* expected = curr;
                            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                                return true;
                            }
                            delete new_node;
                            continue;
                        }
                        continue;
                    }
                }
                
                parent = unmarked;
                if (key < unmarked->val) {
                    child_ptr = &unmarked->left;
                    curr = unmarked->left.load(std::memory_order_acquire);
                } else {
                    child_ptr = &unmarked->right;
                    curr = unmarked->right.load(std::memory_order_acquire);
                }
                
                if (curr && is_marked_ref(curr)) {
                    if (parent) {
                        help_remove(parent, curr, child_ptr == &parent->left);
                    }
                    return remove(key);
                }
            }
            return false;
        }
    }
};