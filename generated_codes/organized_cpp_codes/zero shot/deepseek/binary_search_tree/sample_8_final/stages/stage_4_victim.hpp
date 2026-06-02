#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
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
    
    std::atomic<Node*> root;
    
    struct SearchResult {
        Node* parent;
        Node* target;
        Node* grandparent;
    };
    
    SearchResult search(int key) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* parent = nullptr;
        Node* current = root.load(std::memory_order_acquire);
        
        while (current != nullptr) {
            Node* unmarked = get_unmarked_ref(current);
            int current_val = unmarked->val;
            
            if (current_val == key) {
                result.parent = parent;
                result.target = current;
                return result;
            }
            
            parent = current;
            if (key < current_val) {
                current = unmarked->left.load(std::memory_order_acquire);
            } else {
                current = unmarked->right.load(std::memory_order_acquire);
            }
        }
        
        result.parent = parent;
        result.target = nullptr;
        return result;
    }
    
    bool help_remove(Node* parent, Node* target) {
        Node* unmarked_target = get_unmarked_ref(target);
        int key = unmarked_target->val;
        
        std::atomic<Node*>* child_ptr = nullptr;
        if (parent == nullptr) {
            child_ptr = &root;
        } else {
            Node* unmarked_parent = get_unmarked_ref(parent);
            if (key < unmarked_parent->val) {
                child_ptr = &unmarked_parent->left;
            } else {
                child_ptr = &unmarked_parent->right;
            }
        }
        
        Node* left = unmarked_target->left.load(std::memory_order_acquire);
        Node* right = unmarked_target->right.load(std::memory_order_acquire);
        
        if (left == nullptr && right == nullptr) {
            Node* marked = get_marked_ref(unmarked_target);
            return child_ptr->compare_exchange_strong(target, nullptr, 
                std::memory_order_acq_rel, std::memory_order_relaxed);
        } else if (left == nullptr || right == nullptr) {
            Node* child = (left != nullptr) ? left : right;
            Node* marked = get_marked_ref(unmarked_target);
            return child_ptr->compare_exchange_strong(target, child, 
                std::memory_order_acq_rel, std::memory_order_relaxed);
        } else {
            Node* successor_parent = unmarked_target;
            Node* successor = right;
            
            while (true) {
                Node* unmarked_successor = get_unmarked_ref(successor);
                Node* left_child = unmarked_successor->left.load(std::memory_order_acquire);
                
                if (left_child == nullptr) {
                    break;
                }
                
                successor_parent = successor;
                successor = left_child;
            }
            
            Node* unmarked_successor = get_unmarked_ref(successor);
            Node* successor_right = unmarked_successor->right.load(std::memory_order_acquire);
            
            if (successor_parent == unmarked_target) {
                unmarked_target->right.store(successor_right, std::memory_order_release);
            } else {
                Node* unmarked_successor_parent = get_unmarked_ref(successor_parent);
                unmarked_successor_parent->left.store(successor_right, std::memory_order_release);
            }
            
            unmarked_target->val = unmarked_successor->val;
            
            Node* marked_successor = get_marked_ref(unmarked_successor);
            std::atomic<Node*>* succ_child_ptr = nullptr;
            if (successor_parent == unmarked_target) {
                succ_child_ptr = &unmarked_target->right;
            } else {
                Node* unmarked_successor_parent = get_unmarked_ref(successor_parent);
                succ_child_ptr = &unmarked_successor_parent->left;
            }
            
            return succ_child_ptr->compare_exchange_strong(successor, successor_right,
                std::memory_order_acq_rel, std::memory_order_relaxed);
        }
    }
    
    void delete_subtree(Node* node) {
        if (node == nullptr || is_marked_ref(node)) {
            return;
        }
        
        Node* unmarked = get_unmarked_ref(node);
        delete_subtree(unmarked->left.load(std::memory_order_relaxed));
        delete_subtree(unmarked->right.load(std::memory_order_relaxed));
        delete unmarked;
    }
    
public:
    ConcurrentDataStructure() {
        root.store(nullptr, std::memory_order_relaxed);
    }
    
    ~ConcurrentDataStructure() override {
        delete_subtree(root.load(std::memory_order_relaxed));
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        
        while (current != nullptr) {
            Node* unmarked = get_unmarked_ref(current);
            int current_val = unmarked->val;
            
            if (current_val == key) {
                return !is_marked_ref(current);
            }
            
            if (key < current_val) {
                current = unmarked->left.load(std::memory_order_acquire);
            } else {
                current = unmarked->right.load(std::memory_order_acquire);
            }
        }
        
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            SearchResult result = search(key);
            
            if (result.target != nullptr) {
                if (is_marked_ref(result.target)) {
                    help_remove(result.parent, result.target);
                    continue;
                }
                return false;
            }
            
            Node* new_node = new Node(key);
            std::atomic<Node*>* child_ptr = nullptr;
            
            if (result.parent == nullptr) {
                child_ptr = &root;
            } else {
                Node* unmarked_parent = get_unmarked_ref(result.parent);
                if (key < unmarked_parent->val) {
                    child_ptr = &unmarked_parent->left;
                } else {
                    child_ptr = &unmarked_parent->right;
                }
            }
            
            Node* expected = nullptr;
            if (child_ptr->compare_exchange_strong(expected, new_node,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            SearchResult result = search(key);
            
            if (result.target == nullptr) {
                return false;
            }
            
            if (is_marked_ref(result.target)) {
                help_remove(result.parent, result.target);
                continue;
            }
            
            Node* unmarked_target = get_unmarked_ref(result.target);
            
            Node* left = unmarked_target->left.load(std::memory_order_acquire);
            Node* right = unmarked_target->right.load(std::memory_order_acquire);
            
            if (left == nullptr && right == nullptr) {
                Node* marked = get_marked_ref(unmarked_target);
                std::atomic<Node*>* child_ptr = nullptr;
                
                if (result.parent == nullptr) {
                    child_ptr = &root;
                } else {
                    Node* unmarked_parent = get_unmarked_ref(result.parent);
                    if (key < unmarked_parent->val) {
                        child_ptr = &unmarked_parent->left;
                    } else {
                        child_ptr = &unmarked_parent->right;
                    }
                }
                
                if (child_ptr->compare_exchange_strong(result.target, nullptr,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    delete unmarked_target;
                    return true;
                }
            } else if (left == nullptr || right == nullptr) {
                Node* child = (left != nullptr) ? left : right;
                Node* marked = get_marked_ref(unmarked_target);
                std::atomic<Node*>* child_ptr = nullptr;
                
                if (result.parent == nullptr) {
                    child_ptr = &root;
                } else {
                    Node* unmarked_parent = get_unmarked_ref(result.parent);
                    if (key < unmarked_parent->val) {
                        child_ptr = &unmarked_parent->left;
                    } else {
                        child_ptr = &unmarked_parent->right;
                    }
                }
                
                if (child_ptr->compare_exchange_strong(result.target, child,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    delete unmarked_target;
                    return true;
                }
            } else {
                if (help_remove(result.parent, result.target)) {
                    return true;
                }
            }
        }
    }
};