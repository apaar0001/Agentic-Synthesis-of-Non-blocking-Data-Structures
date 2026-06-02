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
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    Node* findMin(Node* node) {
        Node* curr = node;
        while (curr) {
            Node* unmarkedCurr = get_unmarked_ref(curr);
            if (!unmarkedCurr) break;
            
            Node* left = unmarkedCurr->left.load(std::memory_order_acquire);
            if (!left) break;
            curr = left;
        }
        return curr;
    }
    
    void helpRemove(Node* node) {
        if (!node) return;
        Node* unmarkedNode = get_unmarked_ref(node);
        if (!unmarkedNode) return;
        
        Node* left = unmarkedNode->left.load(std::memory_order_acquire);
        Node* right = unmarkedNode->right.load(std::memory_order_acquire);
        
        if (!get_unmarked_ref(left) || !get_unmarked_ref(right)) {
            Node* child = get_unmarked_ref(left) ? left : right;
            Node* markedNode = get_marked_ref(unmarkedNode);
            Node* expected = markedNode;
            if (unmarkedNode->val < root.load(std::memory_order_acquire)->val) {
                root.load(std::memory_order_acquire)->left.compare_exchange_strong(expected, child, std::memory_order_acq_rel);
            } else {
                root.load(std::memory_order_acquire)->right.compare_exchange_strong(expected, child, std::memory_order_acq_rel);
            }
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
            Node* unmarkedCurr = get_unmarked_ref(curr);
            if (!unmarkedCurr) {
                curr = root.load(std::memory_order_acquire);
                continue;
            }
            
            if (unmarkedCurr->val == key) {
                return !is_marked_ref(curr);
            } else if (key < unmarkedCurr->val) {
                curr = unmarkedCurr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarkedCurr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* parentField = &root;
            Node* curr = root.load(std::memory_order_acquire);
            
            while (curr) {
                Node* unmarkedCurr = get_unmarked_ref(curr);
                if (!unmarkedCurr) {
                    curr = root.load(std::memory_order_acquire);
                    continue;
                }
                
                if (unmarkedCurr->val == key) {
                    if (is_marked_ref(curr)) {
                        helpRemove(curr);
                        continue;
                    }
                    return false;
                }
                
                parent = curr;
                if (key < unmarkedCurr->val) {
                    parentField = &unmarkedCurr->left;
                    curr = unmarkedCurr->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarkedCurr->right;
                    curr = unmarkedCurr->right.load(std::memory_order_acquire);
                }
            }
            
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (parentField->compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* parentField = &root;
            Node* curr = root.load(std::memory_order_acquire);
            
            while (curr) {
                Node* unmarkedCurr = get_unmarked_ref(curr);
                if (!unmarkedCurr) {
                    curr = root.load(std::memory_order_acquire);
                    continue;
                }
                
                if (unmarkedCurr->val == key) {
                    if (is_marked_ref(curr)) {
                        return false;
                    }
                    
                    Node* markedCurr = get_marked_ref(curr);
                    Node* expected = curr;
                    if (parentField->compare_exchange_strong(expected, markedCurr, std::memory_order_acq_rel)) {
                        // Node has been marked
                        helpRemove(curr);
                        return true;
                    }
                    continue;
                }
                
                parent = curr;
                if (key < unmarkedCurr->val) {
                    parentField = &unmarkedCurr->left;
                    curr = unmarkedCurr->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarkedCurr->right;
                    curr = unmarkedCurr->right.load(std::memory_order_acquire);
                }
            }
            return false;
        }
    }
};