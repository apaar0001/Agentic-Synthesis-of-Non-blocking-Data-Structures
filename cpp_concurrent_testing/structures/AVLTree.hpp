#pragma once
#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>
#include <algorithm>

/*
 * Lock-Based AVL Tree
 * 
 * A self-balancing binary search tree with global locking.
 * Maintains height balance through rotations.
 * 
 * Key features:
 * - Global mutex for thread safety
 * - AVL balance property: |height(left) - height(right)| <= 1
 * - Automatic rebalancing on insert/remove
 */

class AVLTree : public SetADT {
private:
    struct Node {
        int key;
        Node* left;
        Node* right;
        int height;
        
        Node(int k) : key(k), left(nullptr), right(nullptr), height(1) {}
    };
    
    Node* root;
    std::mutex treeMutex;
    
    // Get height of node
    int height(Node* n) const {
        return n ? n->height : 0;
    }
    
    // Update height of node
    void updateHeight(Node* n) {
        if (n) {
            n->height = 1 + std::max(height(n->left), height(n->right));
        }
    }
    
    // Get balance factor
    int getBalance(Node* n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }
    
    // Right rotation
    Node* rotateRight(Node* y) {
        Node* x = y->left;
        Node* T2 = x->right;
        
        x->right = y;
        y->left = T2;
        
        updateHeight(y);
        updateHeight(x);
        
        return x;
    }
    
    // Left rotation
    Node* rotateLeft(Node* x) {
        Node* y = x->right;
        Node* T2 = y->left;
        
        y->left = x;
        x->right = T2;
        
        updateHeight(x);
        updateHeight(y);
        
        return y;
    }
    
    // Insert helper
    Node* insertNode(Node* node, int key, bool& inserted) {
        if (!node) {
            inserted = true;
            return new Node(key);
        }
        
        if (key < node->key) {
            node->left = insertNode(node->left, key, inserted);
        } else if (key > node->key) {
            node->right = insertNode(node->right, key, inserted);
        } else {
            inserted = false;  // Duplicate
            return node;
        }
        
        updateHeight(node);
        
        int balance = getBalance(node);
        
        // Left Left Case
        if (balance > 1 && key < node->left->key) {
            return rotateRight(node);
        }
        
        // Right Right Case
        if (balance < -1 && key > node->right->key) {
            return rotateLeft(node);
        }
        
        // Left Right Case
        if (balance > 1 && key > node->left->key) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        
        // Right Left Case
        if (balance < -1 && key < node->right->key) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    // Find minimum node
    Node* minValueNode(Node* node) {
        Node* current = node;
        while (current->left) {
            current = current->left;
        }
        return current;
    }
    
    // Remove helper
    Node* removeNode(Node* node, int key, bool& removed) {
        if (!node) {
            removed = false;
            return nullptr;
        }
        
        if (key < node->key) {
            node->left = removeNode(node->left, key, removed);
        } else if (key > node->key) {
            node->right = removeNode(node->right, key, removed);
        } else {
            removed = true;
            
            // Node with only one child or no child
            if (!node->left || !node->right) {
                Node* temp = node->left ? node->left : node->right;
                
                if (!temp) {
                    temp = node;
                    node = nullptr;
                } else {
                    *node = *temp;
                }
                
                delete temp;
            } else {
                // Node with two children
                Node* temp = minValueNode(node->right);
                node->key = temp->key;
                node->right = removeNode(node->right, temp->key, removed);
            }
        }
        
        if (!node) {
            return node;
        }
        
        updateHeight(node);
        
        int balance = getBalance(node);
        
        // Left Left Case
        if (balance > 1 && getBalance(node->left) >= 0) {
            return rotateRight(node);
        }
        
        // Left Right Case
        if (balance > 1 && getBalance(node->left) < 0) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        
        // Right Right Case
        if (balance < -1 && getBalance(node->right) <= 0) {
            return rotateLeft(node);
        }
        
        // Right Left Case
        if (balance < -1 && getBalance(node->right) > 0) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    // Contains helper
    bool containsNode(Node* node, int key) const {
        if (!node) {
            return false;
        }
        
        if (key == node->key) {
            return true;
        } else if (key < node->key) {
            return containsNode(node->left, key);
        } else {
            return containsNode(node->right, key);
        }
    }
    
    // Destructor helper
    void deleteTree(Node* node) {
        if (node) {
            deleteTree(node->left);
            deleteTree(node->right);
            delete node;
        }
    }

public:
    AVLTree() : root(nullptr) {}
    
    ~AVLTree() override {
        deleteTree(root);
    }
    
    bool contains(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return containsNode(root, key);
    }
    
    bool add(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        bool inserted = false;
        root = insertNode(root, key, inserted);
        return inserted;
    }
    
    bool remove(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        bool removed = false;
        root = removeNode(root, key, removed);
        return removed;
    }
};
