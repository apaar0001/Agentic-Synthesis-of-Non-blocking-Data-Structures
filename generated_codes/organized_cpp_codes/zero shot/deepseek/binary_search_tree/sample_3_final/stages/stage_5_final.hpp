#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

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
    
    Node* root;
    Node* left_sentinel;
    Node* right_sentinel;
    
    struct SearchResult {
        Node* parent;
        Node* target;
        Node* gp;
    };
    
    SearchResult search(int key, Node* start = nullptr) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* curr = start ? start : root;
        Node* prev = nullptr;
        Node* gprev = nullptr;
        
        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == left_sentinel || unmarked_curr == right_sentinel) {
                break;
            }
            
            if (is_marked_ref(curr)) {
                if (prev) {
                    Node* unmarked_prev = get_unmarked_ref(prev);
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (unmarked_prev->left.load(std::memory_order_relaxed) == curr) {
                        Node* next = unmarked_curr->right.load(std::memory_order_acquire);
                        if (unmarked_prev->left.compare_exchange_strong(curr, 
                            next, std::memory_order_acq_rel)) {
                            delete unmarked_curr;
                        }
                    } else if (unmarked_prev->right.load(std::memory_order_relaxed) == curr) {
                        Node* next = unmarked_curr->left.load(std::memory_order_acquire);
                        if (unmarked_prev->right.compare_exchange_strong(curr, 
                            next, std::memory_order_acq_rel)) {
                            delete unmarked_curr;
                        }
                    }
                }
                curr = root;
                prev = nullptr;
                gprev = nullptr;
                continue;
            }
            
            if (unmarked_curr->val == key) {
                result.parent = prev;
                result.target = curr;
                result.gp = gprev;
                return result;
            }
            
            gprev = prev;
            prev = curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }
        
        result.parent = prev;
        result.target = curr;
        result.gp = gprev;
        return result;
    }
    
    void cleanup_marked_nodes(Node* node) {
        if (!node || node == left_sentinel || node == right_sentinel) return;
        
        Node* unmarked = get_unmarked_ref(node);
        if (is_marked_ref(node)) {
            delete unmarked;
            return;
        }
        
        Node* left = unmarked->left.load(std::memory_order_relaxed);
        Node* right = unmarked->right.load(std::memory_order_relaxed);
        
        cleanup_marked_nodes(get_unmarked_ref(left));
        cleanup_marked_nodes(get_unmarked_ref(right));
        
        delete unmarked;
    }
    
public:
    ConcurrentDataStructure() {
        left_sentinel = new Node(INT_MIN);
        right_sentinel = new Node(INT_MAX);
        root = new Node((INT_MIN + INT_MAX) / 2, left_sentinel, right_sentinel);
    }
    
    ~ConcurrentDataStructure() override {
        cleanup_marked_nodes(root);
        delete left_sentinel;
        delete right_sentinel;
    }
    
    bool contains(int key) override {
        SearchResult res = search(key);
        if (!res.target) return false;
        
        Node* unmarked_target = get_unmarked_ref(res.target);
        return !is_marked_ref(res.target) && unmarked_target->val == key;
    }
    
    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);
            
            if (res.target) {
                Node* unmarked_target = get_unmarked_ref(res.target);
                if (!is_marked_ref(res.target) && unmarked_target->val == key) {
                    return false;
                }
            }
            
            if (!res.parent) continue;
            
            Node* unmarked_parent = get_unmarked_ref(res.parent);
            if (is_marked_ref(res.parent)) continue;
            
            Node* new_node = new Node(key);
            if (key < unmarked_parent->val) {
                Node* expected = unmarked_parent->left.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) continue;
                new_node->left.store(expected, std::memory_order_relaxed);
                new_node->right.store(unmarked_parent, std::memory_order_relaxed);
                if (unmarked_parent->left.compare_exchange_strong(expected, 
                    new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
            } else {
                Node* expected = unmarked_parent->right.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) continue;
                new_node->left.store(unmarked_parent, std::memory_order_relaxed);
                new_node->right.store(expected, std::memory_order_relaxed);
                if (unmarked_parent->right.compare_exchange_strong(expected, 
                    new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            
            if (!res.target) return false;
            
            Node* unmarked_target = get_unmarked_ref(res.target);
            if (is_marked_ref(res.target) || unmarked_target->val != key) {
                return false;
            }
            
            if (!res.parent) continue;
            
            Node* unmarked_parent = get_unmarked_ref(res.parent);
            if (is_marked_ref(res.parent)) continue;
            
            Node* marked_target = get_marked_ref(res.target);
            if (key < unmarked_parent->val) {
                if (unmarked_parent->left.compare_exchange_strong(res.target, 
                    marked_target, std::memory_order_acq_rel)) {
                    Node* left = unmarked_target->left.load(std::memory_order_acquire);
                    Node* right = unmarked_target->right.load(std::memory_order_acquire);
                    Node* replacement = (left && !is_marked_ref(left)) ? left : right;
                    if (replacement && !is_marked_ref(replacement)) {
                        unmarked_parent->left.compare_exchange_strong(marked_target, 
                            replacement, std::memory_order_acq_rel);
                    }
                    return true;
                }
            } else {
                if (unmarked_parent->right.compare_exchange_strong(res.target, 
                    marked_target, std::memory_order_acq_rel)) {
                    Node* left = unmarked_target->left.load(std::memory_order_acquire);
                    Node* right = unmarked_target->right.load(std::memory_order_acquire);
                    Node* replacement = (left && !is_marked_ref(left)) ? left : right;
                    if (replacement && !is_marked_ref(replacement)) {
                        unmarked_parent->right.compare_exchange_strong(marked_target, 
                            replacement, std::memory_order_acq_rel);
                    }
                    return true;
                }
            }
        }
    }
};