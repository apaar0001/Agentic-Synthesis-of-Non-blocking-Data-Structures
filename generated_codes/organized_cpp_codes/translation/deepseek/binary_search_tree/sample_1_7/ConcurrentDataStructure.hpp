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
            delete get_unmarked_ref(current);
        }
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        while (current) {
            Node* unmarked_current = get_unmarked_ref(current);
            if (unmarked_current->val == key) {
                return !is_marked_ref(current);
            } else if (key < unmarked_current->val) {
                current = unmarked_current->left.load(std::memory_order_acquire);
            } else {
                current = unmarked_current->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            bool is_left = false;
            Node* current = root.load(std::memory_order_acquire);
            
            while (current) {
                Node* unmarked_current = get_unmarked_ref(current);
                
                if (key == unmarked_current->val) {
                    if (is_marked_ref(current)) {
                        break;
                    }
                    return false;
                }
                
                parent = unmarked_current;
                if (key < unmarked_current->val) {
                    Node* next = unmarked_current->left.load(std::memory_order_acquire);
                    if (next && is_marked_ref(next)) {
                        break;
                    }
                    current = next;
                    is_left = true;
                } else {
                    Node* next = unmarked_current->right.load(std::memory_order_acquire);
                    if (next && is_marked_ref(next)) {
                        break;
                    }
                    current = next;
                    is_left = false;
                }
            }
            
            Node* new_node = new Node(key);
            
            if (!parent) {
                Node* expected = nullptr;
                if (root.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
                continue;
            }
            
            std::atomic<Node*>* child_ptr = is_left ? &(parent->left) : &(parent->right);
            Node* expected = child_ptr->load(std::memory_order_acquire);
            
            if (expected && is_marked_ref(expected)) {
                delete new_node;
                continue;
            }
            
            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            bool is_left = false;
            Node* current = root.load(std::memory_order_acquire);
            
            while (current) {
                Node* unmarked_current = get_unmarked_ref(current);
                
                if (key == unmarked_current->val) {
                    if (is_marked_ref(current)) {
                        return false;
                    }
                    
                    if (!parent) {
                        Node* marked = get_marked_ref(unmarked_current);
                        if (root.compare_exchange_strong(current, marked, std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                        continue;
                    }
                    
                    std::atomic<Node*>* child_ptr = is_left ? &(parent->left) : &(parent->right);
                    Node* marked = get_marked_ref(unmarked_current);
                    if (child_ptr->compare_exchange_strong(current, marked, std::memory_order_acq_rel)) {
                        // Node has been marked
                        return true;
                    }
                    continue;
                }
                
                parent = unmarked_current;
                if (key < unmarked_current->val) {
                    Node* next = unmarked_current->left.load(std::memory_order_acquire);
                    if (next && is_marked_ref(next)) {
                        break;
                    }
                    current = next;
                    is_left = true;
                } else {
                    Node* next = unmarked_current->right.load(std::memory_order_acquire);
                    if (next && is_marked_ref(next)) {
                        break;
                    }
                    current = next;
                    is_left = false;
                }
            }
            return false;
        }
    }
};