#pragma once
#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>

/*
 * SFTree - Speculation-Friendly Tree
 * 
 * A simple binary search tree with a global lock for simplicity.
 * In a full implementation, this would use speculative execution
 * and optimistic concurrency control, but for this port we use
 * a straightforward lock-based approach.
 * 
 * Key features:
 * - Global mutex for thread safety
 * - Standard BST operations
 * - RAII-compliant memory management
 */

class SFTree : public SetADT {
private:
    struct Node {
        int key;
        Node* left;
        Node* right;
        
        Node(int k) : key(k), left(nullptr), right(nullptr) {}
    };
    
    Node* root;
    std::mutex treeMutex;
    
    // Helper: search for a key
    bool searchHelper(Node* node, int key) {
        if (!node) return false;
        if (key == node->key) return true;
        if (key < node->key) return searchHelper(node->left, key);
        return searchHelper(node->right, key);
    }
    
    // Helper: insert a key
    bool insertHelper(Node*& node, int key) {
        if (!node) {
            node = new Node(key);
            return true;
        }
        if (key == node->key) return false;
        if (key < node->key) return insertHelper(node->left, key);
        return insertHelper(node->right, key);
    }
    
    // Helper: find minimum node
    Node* findMin(Node* node) {
        while (node && node->left) {
            node = node->left;
        }
        return node;
    }
    
    // Helper: delete a key
    bool deleteHelper(Node*& node, int key) {
        if (!node) return false;
        
        if (key < node->key) {
            return deleteHelper(node->left, key);
        } else if (key > node->key) {
            return deleteHelper(node->right, key);
        } else {
            // Found the node to delete
            if (!node->left && !node->right) {
                // Leaf node
                delete node;
                node = nullptr;
            } else if (!node->left) {
                // Only right child
                Node* temp = node;
                node = node->right;
                delete temp;
            } else if (!node->right) {
                // Only left child
                Node* temp = node;
                node = node->left;
                delete temp;
            } else {
                // Two children: find inorder successor
                Node* successor = findMin(node->right);
                node->key = successor->key;
                deleteHelper(node->right, successor->key);
            }
            return true;
        }
    }
    
    // Helper: destroy tree
    void destroyTree(Node* node) {
        if (!node) return;
        destroyTree(node->left);
        destroyTree(node->right);
        delete node;
    }

public:
    SFTree() : root(nullptr) {}
    
    ~SFTree() override {
        destroyTree(root);
    }
    
    bool contains(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return searchHelper(root, key);
    }
    
    bool add(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return insertHelper(root, key);
    }
    
    bool remove(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return deleteHelper(root, key);
    }
};
