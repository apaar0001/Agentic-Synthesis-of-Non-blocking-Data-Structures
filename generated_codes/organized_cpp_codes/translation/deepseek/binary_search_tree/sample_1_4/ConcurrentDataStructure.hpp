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
    
    std::atomic<Node*> root;
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1);
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() {
        Node* r = root.load(std::memory_order_relaxed);
        if (r) {
            delete get_unmarked_ref(r);
        }
    }
    
    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (!unmarked_curr) break;
            
            if (key == unmarked_curr->val) {
                return !is_marked_ref(curr);
            } else if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* parent_ref = &root;
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (!unmarked_curr) {
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parent_ref = &root;
                    continue;
                }
                
                if (key == unmarked_curr->val) {
                    if (is_marked_ref(curr)) {
                        Node* new_curr = get_unmarked_ref(curr);
                        if (parent_ref->compare_exchange_strong(curr, new_curr,
                                                               std::memory_order_acq_rel,
                                                               std::memory_order_acquire)) {
                            curr = new_curr;
                            continue;
                        } else {
                            curr = root.load(std::memory_order_acquire);
                            parent = nullptr;
                            parent_ref = &root;
                            continue;
                        }
                    }
                    return false;
                }
                
                parent = unmarked_curr;
                if (key < unmarked_curr->val) {
                    parent_ref = &unmarked_curr->left;
                    curr = unmarked_curr->left.load(std::memory_order_acquire);
                } else {
                    parent_ref = &unmarked_curr->right;
                    curr = unmarked_curr->right.load(std::memory_order_acquire);
                }
            }
            
            Node* new_node = new Node(key);
            Node* expected = curr;
            if (parent_ref->compare_exchange_strong(expected, new_node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* parent_ref = &root;
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (!unmarked_curr) {
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parent_ref = &root;
                    continue;
                }
                
                if (key == unmarked_curr->val) {
                    if (is_marked_ref(curr)) {
                        return false;
                    }
                    
                    Node* marked = get_marked_ref(unmarked_curr);
                    if (parent_ref->compare_exchange_strong(curr, marked,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                        // Node has been marked
                        
                        Node* left = unmarked_curr->left.load(std::memory_order_acquire);
                        Node* right = unmarked_curr->right.load(std::memory_order_acquire);
                        
                        if (!left && !right) {
                            Node* unmarked_marked = get_unmarked_ref(marked);
                            if (parent_ref->compare_exchange_strong(marked, nullptr,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                                return true;
                            }
                        } else if (!left) {
                            Node* unmarked_right = get_unmarked_ref(right);
                            if (parent_ref->compare_exchange_strong(marked, unmarked_right,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                                return true;
                            }
                        } else if (!right) {
                            Node* unmarked_left = get_unmarked_ref(left);
                            if (parent_ref->compare_exchange_strong(marked, unmarked_left,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                                return true;
                            }
                        } else {
                            while (true) {
                                Node* successor_parent = unmarked_curr;
                                std::atomic<Node*>* successor_parent_ref = &unmarked_curr->right;
                                Node* successor = unmarked_curr->right.load(std::memory_order_acquire);
                                
                                while (successor) {
                                    Node* unmarked_successor = get_unmarked_ref(successor);
                                    if (!unmarked_successor) {
                                        break;
                                    }
                                    
                                    Node* successor_left = unmarked_successor->left.load(std::memory_order_acquire);
                                    if (!successor_left) {
                                        int successor_val = unmarked_successor->val;
                                        Node* marked_successor = get_marked_ref(unmarked_successor);
                                        if (successor_parent_ref->compare_exchange_strong(successor, marked_successor,
                                                                                         std::memory_order_acq_rel,
                                                                                         std::memory_order_acquire)) {
                                            unmarked_curr->val = successor_val;
                                            
                                            Node* successor_right = unmarked_successor->right.load(std::memory_order_acquire);
                                            if (successor_right) {
                                                Node* unmarked_right = get_unmarked_ref(successor_right);
                                                if (successor_parent_ref->compare_exchange_strong(marked_successor, unmarked_right,
                                                                                                 std::memory_order_acq_rel,
                                                                                                 std::memory_order_acquire)) {
                                                    return true;
                                                }
                                            } else {
                                                if (successor_parent_ref->compare_exchange_strong(marked_successor, nullptr,
                                                                                                 std::memory_order_acq_rel,
                                                                                                 std::memory_order_acquire)) {
                                                    return true;
                                                }
                                            }
                                            break;
                                        } else {
                                            break;
                                        }
                                    } else {
                                        successor_parent = unmarked_successor;
                                        successor_parent_ref = &unmarked_successor->left;
                                        successor = successor_left;
                                    }
                                }
                                
                                if (!successor) {
                                    break;
                                }
                            }
                        }
                        continue;
                    } else {
                        curr = root.load(std::memory_order_acquire);
                        parent = nullptr;
                        parent_ref = &root;
                        continue;
                    }
                }
                
                parent = unmarked_curr;
                if (key < unmarked_curr->val) {
                    parent_ref = &unmarked_curr->left;
                    curr = unmarked_curr->left.load(std::memory_order_acquire);
                } else {
                    parent_ref = &unmarked_curr->right;
                    curr = unmarked_curr->right.load(std::memory_order_acquire);
                }
            }
            
            return false;
        }
    }
};