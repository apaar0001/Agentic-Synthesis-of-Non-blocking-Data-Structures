#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

/*
 * LockFreeBSTree - Lock-free Binary Search Tree
 * 
 * A lock-free binary search tree using atomic operations.
 * This is a simplified implementation using atomic pointers
 * for children and marked pointers for logical deletion.
 * 
 * Key features:
 * - Lock-free operations using CAS
 * - Atomic child pointers
 * - Marked pointers for logical deletion
 * - RAII-compliant memory management
 */

class LockFreeBSTree : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        std::atomic<bool> marked;
        
        Node(int k) : key(k), left(nullptr), right(nullptr), marked(false) {}
    };
    
    Node* root;
    
    // Helper: search for a key
    bool searchHelper(Node* node, int key) {
        while (node) {
            if (node->marked.load(std::memory_order_acquire)) {
                return false;
            }
            if (key == node->key) {
                return !node->marked.load(std::memory_order_acquire);
            }
            if (key < node->key) {
                node = node->left.load(std::memory_order_acquire);
            } else {
                node = node->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    // Helper: insert a key (simplified lock-free approach)
    bool insertHelper(Node* node, int key) {
        while (true) {
            if (!node) return false;
            
            if (node->marked.load(std::memory_order_acquire)) {
                return false;
            }
            
            if (key == node->key) {
                return false;
            }
            
            if (key < node->key) {
                Node* left = node->left.load(std::memory_order_acquire);
                if (!left) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (node->left.compare_exchange_strong(expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    delete newNode;
                    continue;
                }
                node = left;
            } else {
                Node* right = node->right.load(std::memory_order_acquire);
                if (!right) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (node->right.compare_exchange_strong(expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    }
                    delete newNode;
                    continue;
                }
                node = right;
            }
        }
    }
    
    // Helper: delete a key (simplified - just mark as deleted)
    bool deleteHelper(Node* node, int key) {
        while (node) {
            if (node->marked.load(std::memory_order_acquire)) {
                return false;
            }
            
            if (key == node->key) {
                bool expected = false;
                if (node->marked.compare_exchange_strong(expected, true,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                return false;
            }
            
            if (key < node->key) {
                node = node->left.load(std::memory_order_acquire);
            } else {
                node = node->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    // Helper: destroy tree
    void destroyTree(Node* node) {
        if (!node) return;
        Node* left = node->left.load(std::memory_order_relaxed);
        Node* right = node->right.load(std::memory_order_relaxed);
        delete node;
        destroyTree(left);
        destroyTree(right);
    }

public:
    LockFreeBSTree() {
        // Create root with sentinel value
        root = new Node(INT_MIN);
    }
    
    ~LockFreeBSTree() override {
        destroyTree(root);
    }
    
    bool contains(int key) override {
        if (key == INT_MIN) return false;
        return searchHelper(root, key);
    }
    
    bool add(int key) override {
        if (key == INT_MIN) return false;
        return insertHelper(root, key);
    }
    
    bool remove(int key) override {
        if (key == INT_MIN) return false;
        return deleteHelper(root, key);
    }
};
