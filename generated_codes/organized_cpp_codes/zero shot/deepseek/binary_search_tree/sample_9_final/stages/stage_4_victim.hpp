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
        
        Node(int v, Node* l = nullptr, Node* r = nullptr) : val(v), left(l), right(r) {}
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

    bool search(int key, Node** parent_ptr, Node** target_ptr) {
        Node* parent = nullptr;
        Node* current = root.load(std::memory_order_acquire);
        
        while (current != nullptr) {
            Node* unmarked_current = get_unmarked_ref(current);
            if (unmarked_current == nullptr) break;
            
            if (is_marked_ref(current)) {
                if (parent == nullptr) {
                    Node* expected = current;
                    Node* desired = get_unmarked_ref(current);
                    root.compare_exchange_strong(expected, desired, 
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    current = root.load(std::memory_order_acquire);
                    continue;
                }
                
                Node* unmarked_parent = get_unmarked_ref(parent);
                Node* next = (key < unmarked_parent->val) ? 
                    unmarked_parent->left.load(std::memory_order_acquire) : 
                    unmarked_parent->right.load(std::memory_order_acquire);
                
                if (next == current) {
                    Node* marked_next = get_marked_ref(unmarked_current);
                    if (key < unmarked_parent->val) {
                        unmarked_parent->left.compare_exchange_strong(next, marked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    } else {
                        unmarked_parent->right.compare_exchange_strong(next, marked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                }
                
                current = root.load(std::memory_order_acquire);
                parent = nullptr;
                continue;
            }
            
            if (unmarked_current->val == key) {
                *parent_ptr = parent;
                *target_ptr = current;
                return true;
            }
            
            parent = current;
            current = (key < unmarked_current->val) ? 
                unmarked_current->left.load(std::memory_order_acquire) : 
                unmarked_current->right.load(std::memory_order_acquire);
        }
        
        *parent_ptr = parent;
        *target_ptr = current;
        return false;
    }

    void cleanup(Node* node) {
        if (node == nullptr) return;
        Node* unmarked = get_unmarked_ref(node);
        if (unmarked) {
            cleanup(unmarked->left.load(std::memory_order_acquire));
            cleanup(unmarked->right.load(std::memory_order_acquire));
            delete unmarked;
        }
    }

public:
    ConcurrentDataStructure() {
        root.store(nullptr, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        cleanup(root.load(std::memory_order_acquire));
    }

    bool contains(int key) override {
        Node* parent;
        Node* target;
        return search(key, &parent, &target);
    }

    bool add(int key) override {
        while (true) {
            Node* parent;
            Node* target;
            
            if (search(key, &parent, &target)) {
                return false;
            }
            
            Node* new_node = new Node(key);
            
            if (parent == nullptr) {
                Node* expected = nullptr;
                if (root.compare_exchange_strong(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete new_node;
                continue;
            }
            
            Node* unmarked_parent = get_unmarked_ref(parent);
            if (is_marked_ref(parent)) {
                delete new_node;
                continue;
            }
            
            std::atomic<Node*>* child_ptr = (key < unmarked_parent->val) ? 
                &unmarked_parent->left : &unmarked_parent->right;
            
            Node* expected = child_ptr->load(std::memory_order_acquire);
            if (expected != target || is_marked_ref(expected)) {
                delete new_node;
                continue;
            }
            
            if (child_ptr->compare_exchange_strong(expected, new_node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent;
            Node* target;
            
            if (!search(key, &parent, &target)) {
                return false;
            }
            
            Node* unmarked_target = get_unmarked_ref(target);
            if (is_marked_ref(target)) {
                return false;
            }
            
            Node* left = unmarked_target->left.load(std::memory_order_acquire);
            Node* right = unmarked_target->right.load(std::memory_order_acquire);
            
            if (left != nullptr && right != nullptr) {
                Node* successor_parent = unmarked_target;
                Node* successor = right;
                
                while (true) {
                    Node* unmarked_successor = get_unmarked_ref(successor);
                    if (unmarked_successor == nullptr) break;
                    
                    Node* next = unmarked_successor->left.load(std::memory_order_acquire);
                    if (next == nullptr) break;
                    
                    successor_parent = successor;
                    successor = next;
                }
                
                unmarked_target->val = get_unmarked_ref(successor)->val;
                parent = successor_parent;
                target = successor;
                unmarked_target = get_unmarked_ref(target);
            }
            
            Node* left_child = unmarked_target->left.load(std::memory_order_acquire);
            Node* right_child = unmarked_target->right.load(std::memory_order_acquire);
            Node* single_child = (left_child != nullptr) ? left_child : right_child;
            
            if (parent == nullptr) {
                if (single_child == nullptr) {
                    Node* expected = unmarked_target;
                    if (root.compare_exchange_strong(expected, nullptr,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                            std::this_thread::sleep_for(std::chrono::seconds(5));
                        delete unmarked_target;
                        return true;
                    }
                    continue;
                }
                
                Node* expected = unmarked_target;
                if (root.compare_exchange_strong(expected, single_child,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    delete unmarked_target;
                    return true;
                }
                continue;
            }
            
            Node* unmarked_parent = get_unmarked_ref(parent);
            if (is_marked_ref(parent)) {
                continue;
            }
            
            std::atomic<Node*>* child_ptr = (unmarked_target->val < unmarked_parent->val) ? 
                &unmarked_parent->left : &unmarked_parent->right;
            
            Node* expected = target;
            Node* marked_target = get_marked_ref(unmarked_target);
            
            if (!child_ptr->compare_exchange_strong(expected, marked_target,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            expected = marked_target;
            if (child_ptr->compare_exchange_strong(expected, single_child,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                delete unmarked_target;
            } else {
                child_ptr->compare_exchange_strong(expected, single_child,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            
            return true;
        }
    }
};