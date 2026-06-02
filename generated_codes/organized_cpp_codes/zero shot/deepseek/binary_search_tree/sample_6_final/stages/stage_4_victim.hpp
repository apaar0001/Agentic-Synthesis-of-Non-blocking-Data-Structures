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
        Node* gp;
    };
    
    SearchResult search(int key) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* parent = nullptr;
        Node* gp = nullptr;
        Node* curr = root.load(std::memory_order_acquire);
        
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (is_marked_ref(curr)) {
                Node* next = nullptr;
                if (unmarked_curr->val < key) {
                    next = unmarked_curr->right.load(std::memory_order_acquire);
                } else if (unmarked_curr->val > key) {
                    next = unmarked_curr->left.load(std::memory_order_acquire);
                } else {
                    next = nullptr;
                }
                
                if (parent == nullptr) {
                    root.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    curr = root.load(std::memory_order_acquire);
                } else if (parent->left.load(std::memory_order_acquire) == curr) {
                    parent->left.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    curr = parent->left.load(std::memory_order_acquire);
                } else {
                    parent->right.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    curr = parent->right.load(std::memory_order_acquire);
                }
                continue;
            }
            
            if (unmarked_curr->val == key) {
                result.parent = parent;
                result.target = unmarked_curr;
                result.gp = gp;
                return result;
            }
            
            gp = parent;
            parent = unmarked_curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }
        
        result.parent = parent;
        result.target = nullptr;
        result.gp = gp;
        return result;
    }
    
    void cleanup(Node* node) {
        if (node == nullptr) return;
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        cleanup(get_unmarked_ref(left));
        cleanup(get_unmarked_ref(right));
        delete get_unmarked_ref(node);
    }
    
public:
    ConcurrentDataStructure() {
        root.store(nullptr, std::memory_order_release);
    }
    
    ~ConcurrentDataStructure() override {
        cleanup(root.load(std::memory_order_acquire));
    }
    
    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (is_marked_ref(curr)) {
                curr = (key < unmarked_curr->val) ? 
                       unmarked_curr->left.load(std::memory_order_acquire) : 
                       unmarked_curr->right.load(std::memory_order_acquire);
                continue;
            }
            
            if (unmarked_curr->val == key) {
                return true;
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
            SearchResult res = search(key);
            if (res.target != nullptr) {
                return false;
            }
            
            Node* new_node = new Node(key);
            Node* parent = res.parent;
            
            if (parent == nullptr) {
                if (root.compare_exchange_strong(parent, new_node, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete new_node;
                continue;
            }
            
            std::atomic<Node*>* child_ptr = nullptr;
            if (key < parent->val) {
                child_ptr = &parent->left;
            } else {
                child_ptr = &parent->right;
            }
            
            Node* expected = child_ptr->load(std::memory_order_acquire);
            if (is_marked_ref(expected) || get_unmarked_ref(expected) != nullptr) {
                delete new_node;
                continue;
            }
            
            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (res.target == nullptr) {
                return false;
            }
            
            Node* target = res.target;
            Node* parent = res.parent;
            
            std::atomic<Node*>* child_ptr = nullptr;
            if (parent == nullptr) {
                child_ptr = &root;
            } else if (parent->left.load(std::memory_order_acquire) == target) {
                child_ptr = &parent->left;
            } else {
                child_ptr = &parent->right;
            }
            
            Node* left = target->left.load(std::memory_order_acquire);
            Node* right = target->right.load(std::memory_order_acquire);
            
            if (left == nullptr && right == nullptr) {
                Node* marked_target = get_marked_ref(target);
                if (child_ptr->compare_exchange_strong(target, nullptr, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                continue;
            } else if (left == nullptr || right == nullptr) {
                Node* child = (left != nullptr) ? left : right;
                Node* marked_target = get_marked_ref(target);
                if (child_ptr->compare_exchange_strong(target, child, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                continue;
            } else {
                Node* successor = right;
                Node* succ_parent = target;
                
                while (true) {
                    Node* unmarked_succ = get_unmarked_ref(successor);
                    if (is_marked_ref(successor)) {
                        Node* next = unmarked_succ->left.load(std::memory_order_acquire);
                        if (succ_parent == target) {
                            target->right.compare_exchange_strong(successor, next, std::memory_order_acq_rel);
                        } else {
                            succ_parent->left.compare_exchange_strong(successor, next, std::memory_order_acq_rel);
                        }
                        successor = right;
                        succ_parent = target;
                        continue;
                    }
                    
                    Node* succ_left = unmarked_succ->left.load(std::memory_order_acquire);
                    if (succ_left == nullptr) {
                        break;
                    }
                    succ_parent = unmarked_succ;
                    successor = succ_left;
                }
                
                Node* unmarked_succ = get_unmarked_ref(successor);
                Node* succ_right = unmarked_succ->right.load(std::memory_order_acquire);
                
                if (succ_parent == target) {
                    Node* marked_succ = get_marked_ref(unmarked_succ);
                    if (target->right.compare_exchange_strong(successor, succ_right, std::memory_order_acq_rel)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        target->val = unmarked_succ->val;
                        return true;
                    }
                } else {
                    Node* marked_succ = get_marked_ref(unmarked_succ);
                    if (succ_parent->left.compare_exchange_strong(successor, succ_right, std::memory_order_acq_rel)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        target->val = unmarked_succ->val;
                        return true;
                    }
                }
            }
        }
    }
};