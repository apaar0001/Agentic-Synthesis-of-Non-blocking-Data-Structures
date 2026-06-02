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
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* current = root.load(std::memory_order_relaxed);
        if (current) {
            Node* unmarked = get_unmarked_ref(current);
            delete unmarked;
        }
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        
        while (current) {
            Node* unmarked = get_unmarked_ref(current);
            if (!unmarked) {
                current = root.load(std::memory_order_acquire);
                continue;
            }
            
            if (key == unmarked->val) {
                return !is_marked_ref(current);
            } else if (key < unmarked->val) {
                current = unmarked->left.load(std::memory_order_acquire);
            } else {
                current = unmarked->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* current = root.load(std::memory_order_acquire);
            bool is_left = false;
            
            while (true) {
                if (!current) {
                    Node* new_node = new Node(key);
                    
                    if (!parent) {
                        Node* expected = nullptr;
                        if (root.compare_exchange_strong(expected, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            return true;
                        }
                        delete new_node;
                        break;
                    }
                    
                    Node* unmarked_parent = get_unmarked_ref(parent);
                    if (!unmarked_parent || is_marked_ref(parent)) {
                        delete new_node;
                        break;
                    }
                    
                    std::atomic<Node*>* child_ptr = is_left ? &(unmarked_parent->left) : &(unmarked_parent->right);
                    Node* expected = nullptr;
                    if (child_ptr->compare_exchange_strong(expected, new_node,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    delete new_node;
                    break;
                }
                
                Node* unmarked = get_unmarked_ref(current);
                if (!unmarked) {
                    break;
                }
                
                if (key == unmarked->val) {
                    return !is_marked_ref(current);
                } else if (key < unmarked->val) {
                    parent = current;
                    current = unmarked->left.load(std::memory_order_acquire);
                    is_left = true;
                } else {
                    parent = current;
                    current = unmarked->right.load(std::memory_order_acquire);
                    is_left = false;
                }
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* current = root.load(std::memory_order_acquire);
            bool is_left = false;
            
            while (true) {
                if (!current) {
                    return false;
                }
                
                Node* unmarked = get_unmarked_ref(current);
                if (!unmarked) {
                    break;
                }
                
                if (key == unmarked->val) {
                    if (is_marked_ref(current)) {
                        return false;
                    }
                    
                    if (!parent) {
                        Node* left_child = unmarked->left.load(std::memory_order_acquire);
                        Node* right_child = unmarked->right.load(std::memory_order_acquire);
                        
                        Node* new_child = nullptr;
                        if (!get_unmarked_ref(left_child) && !get_unmarked_ref(right_child)) {
                            new_child = nullptr;
                        } else if (!get_unmarked_ref(left_child)) {
                            new_child = get_unmarked_ref(right_child);
                        } else if (!get_unmarked_ref(right_child)) {
                            new_child = get_unmarked_ref(left_child);
                        } else {
                            Node* successor = get_unmarked_ref(right_child);
                            Node* succ_parent = current;
                            bool succ_is_left = false;
                            
                            while (true) {
                                Node* succ_unmarked = get_unmarked_ref(successor);
                                if (!succ_unmarked) break;
                                
                                Node* succ_left = succ_unmarked->left.load(std::memory_order_acquire);
                                if (!get_unmarked_ref(succ_left)) {
                                    break;
                                }
                                succ_parent = successor;
                                successor = succ_left;
                                succ_is_left = true;
                            }
                            
                            Node* final_succ = get_unmarked_ref(successor);
                            if (!final_succ) {
                                break;
                            }
                            
                            unmarked->val = final_succ->val;
                            key = final_succ->val;
                            parent = succ_parent;
                            current = successor;
                            is_left = succ_is_left;
                            continue;
                        }
                        
                        Node* expected = unmarked;
                        if (root.compare_exchange_strong(expected, new_child,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            // Node has been marked
                            return true;
                        }
                        break;
                    }
                    
                    Node* unmarked_parent = get_unmarked_ref(parent);
                    if (!unmarked_parent || is_marked_ref(parent)) {
                        break;
                    }
                    
                    std::atomic<Node*>* child_ptr = is_left ? &(unmarked_parent->left) : &(unmarked_parent->right);
                    Node* expected = unmarked;
                    Node* desired = get_marked_ref(unmarked);
                    if (child_ptr->compare_exchange_strong(expected, desired,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // Node has been marked
                        Node* left_child = unmarked->left.load(std::memory_order_acquire);
                        Node* right_child = unmarked->right.load(std::memory_order_acquire);
                        
                        Node* new_child = nullptr;
                        if (!get_unmarked_ref(left_child) && !get_unmarked_ref(right_child)) {
                            new_child = nullptr;
                        } else if (!get_unmarked_ref(left_child)) {
                            new_child = get_unmarked_ref(right_child);
                        } else if (!get_unmarked_ref(right_child)) {
                            new_child = get_unmarked_ref(left_child);
                        } else {
                            Node* successor = get_unmarked_ref(right_child);
                            Node* succ_parent = desired;
                            bool succ_is_left = false;
                            
                            while (true) {
                                Node* succ_unmarked = get_unmarked_ref(successor);
                                if (!succ_unmarked) break;
                                
                                Node* succ_left = succ_unmarked->left.load(std::memory_order_acquire);
                                if (!get_unmarked_ref(succ_left)) {
                                    break;
                                }
                                succ_parent = successor;
                                successor = succ_left;
                                succ_is_left = true;
                            }
                            
                            Node* final_succ = get_unmarked_ref(successor);
                            if (!final_succ) {
                                child_ptr->compare_exchange_strong(desired, get_unmarked_ref(right_child),
                                    std::memory_order_release, std::memory_order_relaxed);
                                return true;
                            }
                            
                            unmarked->val = final_succ->val;
                            child_ptr->compare_exchange_strong(desired, get_unmarked_ref(right_child),
                                std::memory_order_release, std::memory_order_relaxed);
                            return true;
                        }
                        
                        child_ptr->compare_exchange_strong(desired, new_child,
                            std::memory_order_release, std::memory_order_relaxed);
                        return true;
                    }
                    break;
                } else if (key < unmarked->val) {
                    parent = current;
                    current = unmarked->left.load(std::memory_order_acquire);
                    is_left = true;
                } else {
                    parent = current;
                    current = unmarked->right.load(std::memory_order_acquire);
                    is_left = false;
                }
            }
        }
    }
};