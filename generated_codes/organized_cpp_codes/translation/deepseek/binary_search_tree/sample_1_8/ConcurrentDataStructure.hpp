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
    
    bool help_remove(Node* parent, std::atomic<Node*>* child_ptr, Node* curr) {
        Node* unmarked_curr = get_unmarked_ref(curr);
        Node* left = unmarked_curr->left.load(std::memory_order_acquire);
        Node* right = unmarked_curr->right.load(std::memory_order_acquire);
        
        Node* new_child = nullptr;
        if (!left) {
            new_child = right;
        } else if (!right) {
            new_child = left;
        } else {
            return false;
        }
        
        Node* expected = curr;
        return child_ptr->compare_exchange_strong(expected, new_child,
                std::memory_order_acq_rel, std::memory_order_acquire);
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
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->val == key) {
                return !is_marked_ref(curr);
            }
            curr = (key < unmarked_curr->val) ? 
                   unmarked_curr->left.load(std::memory_order_acquire) : 
                   unmarked_curr->right.load(std::memory_order_acquire);
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* child_ptr = &root;
            Node* curr = root.load(std::memory_order_acquire);
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->val == key) {
                    if (is_marked_ref(curr)) {
                        if (help_remove(parent, child_ptr, curr)) {
                            continue;
                        }
                        break;
                    }
                    return false;
                }
                
                parent = unmarked_curr;
                if (key < unmarked_curr->val) {
                    child_ptr = &unmarked_curr->left;
                } else {
                    child_ptr = &unmarked_curr->right;
                }
                
                curr = child_ptr->load(std::memory_order_acquire);
                
                if (curr && is_marked_ref(curr)) {
                    help_remove(parent, child_ptr, curr);
                    break;
                }
            }
            
            if (!curr) {
                Node* new_node = new Node(key);
                Node* expected = nullptr;
                if (child_ptr->compare_exchange_strong(expected, new_node,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    return true;
                }
                delete new_node;
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* child_ptr = &root;
            Node* curr = root.load(std::memory_order_acquire);
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->val == key) {
                    if (is_marked_ref(curr)) {
                        return false;
                    }
                    
                    Node* left = unmarked_curr->left.load(std::memory_order_acquire);
                    Node* right = unmarked_curr->right.load(std::memory_order_acquire);
                    
                    if (!left || !right) {
                        Node* marked_curr = get_marked_ref(curr);
                        Node* expected = curr;
                        if (child_ptr->compare_exchange_strong(expected, marked_curr,
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            // Node has been marked
                            help_remove(parent, child_ptr, marked_curr);
                            return true;
                        }
                        continue;
                    } else {
                        Node* successor_parent = unmarked_curr;
                        std::atomic<Node*>* succ_child_ptr = &unmarked_curr->right;
                        Node* successor = right;
                        
                        while (successor) {
                            Node* unmarked_successor = get_unmarked_ref(successor);
                            Node* succ_left = unmarked_successor->left.load(std::memory_order_acquire);
                            
                            if (!succ_left) {
                                Node* marked_successor = get_marked_ref(successor);
                                Node* expected_successor = successor;
                                if (succ_child_ptr->compare_exchange_strong(expected_successor, marked_successor,
                                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                                    // Node has been marked
                                    unmarked_curr->val = unmarked_successor->val;
                                    
                                    Node* succ_right = unmarked_successor->right.load(std::memory_order_acquire);
                                    Node* new_succ_child = succ_right;
                                    if (succ_child_ptr->compare_exchange_strong(marked_successor, new_succ_child,
                                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                                        return true;
                                    }
                                    break;
                                }
                                break;
                            }
                            
                            successor_parent = unmarked_successor;
                            succ_child_ptr = &unmarked_successor->left;
                            successor = succ_left;
                            
                            if (successor && is_marked_ref(successor)) {
                                help_remove(successor_parent, succ_child_ptr, successor);
                                break;
                            }
                        }
                        continue;
                    }
                }
                
                parent = unmarked_curr;
                if (key < unmarked_curr->val) {
                    child_ptr = &unmarked_curr->left;
                } else {
                    child_ptr = &unmarked_curr->right;
                }
                
                curr = child_ptr->load(std::memory_order_acquire);
                
                if (curr && is_marked_ref(curr)) {
                    help_remove(parent, child_ptr, curr);
                    continue;
                }
            }
            
            return false;
        }
    }
};