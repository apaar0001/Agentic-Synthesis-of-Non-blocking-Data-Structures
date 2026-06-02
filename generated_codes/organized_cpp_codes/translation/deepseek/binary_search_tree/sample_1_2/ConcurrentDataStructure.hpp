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
    
    bool help_unlink(Node* parent, Node* child, std::atomic<Node*>& parent_field) {
        Node* unmarked_child = get_unmarked_ref(child);
        Node* marked_child = get_marked_ref(unmarked_child);
        Node* expected = child;
        return parent_field.compare_exchange_strong(expected, marked_child, std::memory_order_acq_rel);
    }
    
    void attempt_physical_unlink(Node* parent, Node* target, std::atomic<Node*>& parent_field) {
        Node* unmarked_target = get_unmarked_ref(target);
        Node* target_left = unmarked_target->left.load(std::memory_order_acquire);
        Node* expected = target;
        parent_field.compare_exchange_strong(expected, get_unmarked_ref(target_left), std::memory_order_acq_rel);
    }
    
    void clean_marked_nodes(Node* start) {
        Node* current = start;
        while (current) {
            Node* unmarked = get_unmarked_ref(current);
            Node* left = unmarked->left.load(std::memory_order_acquire);
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(left);
            } else {
                break;
            }
        }
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* current = root.load(std::memory_order_relaxed);
        while (current) {
            Node* unmarked = get_unmarked_ref(current);
            Node* left = unmarked->left.load(std::memory_order_relaxed);
            delete unmarked;
            current = get_unmarked_ref(left);
        }
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        while (current) {
            Node* unmarked = get_unmarked_ref(current);
            if (is_marked_ref(current)) {
                current = unmarked->left.load(std::memory_order_acquire);
                continue;
            }
            if (key == unmarked->val) {
                Node* right = unmarked->right.load(std::memory_order_acquire);
                return !is_marked_ref(right);
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
            std::atomic<Node*>* parent_field = &root;
            Node* current = root.load(std::memory_order_acquire);
            
            while (current) {
                Node* unmarked = get_unmarked_ref(current);
                if (is_marked_ref(current)) {
                    clean_marked_nodes(current);
                    return add(key);
                }
                
                if (key == unmarked->val) {
                    return false;
                }
                
                parent = current;
                if (key < unmarked->val) {
                    parent_field = &unmarked->left;
                    current = unmarked->left.load(std::memory_order_acquire);
                } else {
                    parent_field = &unmarked->right;
                    current = unmarked->right.load(std::memory_order_acquire);
                }
            }
            
            Node* new_node = new Node(key);
            Node* expected = nullptr;
            if (parent_field->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* parent_field = &root;
            Node* current = root.load(std::memory_order_acquire);
            
            while (current) {
                Node* unmarked = get_unmarked_ref(current);
                if (is_marked_ref(current)) {
                    clean_marked_nodes(current);
                    return remove(key);
                }
                
                if (key == unmarked->val) {
                    Node* right_ptr = unmarked->right.load(std::memory_order_acquire);
                    if (is_marked_ref(right_ptr)) {
                        return false;
                    }
                    
                    Node* marked_right = get_marked_ref(get_unmarked_ref(right_ptr));
                    Node* expected = right_ptr;
                    if (unmarked->right.compare_exchange_strong(expected, marked_right, std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (parent) {
                            Node* unmarked_parent = get_unmarked_ref(parent);
                            std::atomic<Node*>* field = (unmarked_parent->left.load(std::memory_order_acquire) == current) ? &unmarked_parent->left : &unmarked_parent->right;
                            attempt_physical_unlink(parent, current, *field);
                        } else {
                            attempt_physical_unlink(nullptr, current, root);
                        }
                        return true;
                    }
                    continue;
                }
                
                parent = current;
                if (key < unmarked->val) {
                    parent_field = &unmarked->left;
                    current = unmarked->left.load(std::memory_order_acquire);
                } else {
                    parent_field = &unmarked->right;
                    current = unmarked->right.load(std::memory_order_acquire);
                }
            }
            return false;
        }
    }
    
    ConcurrentDataStructure(const ConcurrentDataStructure&) = delete;
    ConcurrentDataStructure& operator=(const ConcurrentDataStructure&) = delete;
};