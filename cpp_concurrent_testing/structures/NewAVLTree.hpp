#pragma once
#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>
#include <algorithm>

/*
 * New AVL Tree - Improved lock-based AVL tree
 * 
 * Similar to AVLTree but with potential optimizations.
 * For this port, we use the same implementation as AVLTree.
 */

class NewAVLTree : public SetADT {
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
    
    int height(Node* n) const {
        return n ? n->height : 0;
    }
    
    void updateHeight(Node* n) {
        if (n) {
            n->height = 1 + std::max(height(n->left), height(n->right));
        }
    }
    
    int getBalance(Node* n) const {
        return n ? height(n->left) - height(n->right) : 0;
    }
    
    Node* rotateRight(Node* y) {
        Node* x = y->left;
        Node* T2 = x->right;
        x->right = y;
        y->left = T2;
        updateHeight(y);
        updateHeight(x);
        return x;
    }
    
    Node* rotateLeft(Node* x) {
        Node* y = x->right;
        Node* T2 = y->left;
        y->left = x;
        x->right = T2;
        updateHeight(x);
        updateHeight(y);
        return y;
    }
    
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
            inserted = false;
            return node;
        }
        
        updateHeight(node);
        int balance = getBalance(node);
        
        if (balance > 1 && key < node->left->key) return rotateRight(node);
        if (balance < -1 && key > node->right->key) return rotateLeft(node);
        if (balance > 1 && key > node->left->key) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        if (balance < -1 && key < node->right->key) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    Node* minValueNode(Node* node) {
        Node* current = node;
        while (current->left) current = current->left;
        return current;
    }
    
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
                Node* temp = minValueNode(node->right);
                node->key = temp->key;
                node->right = removeNode(node->right, temp->key, removed);
            }
        }
        
        if (!node) return node;
        
        updateHeight(node);
        int balance = getBalance(node);
        
        if (balance > 1 && getBalance(node->left) >= 0) return rotateRight(node);
        if (balance > 1 && getBalance(node->left) < 0) {
            node->left = rotateLeft(node->left);
            return rotateRight(node);
        }
        if (balance < -1 && getBalance(node->right) <= 0) return rotateLeft(node);
        if (balance < -1 && getBalance(node->right) > 0) {
            node->right = rotateRight(node->right);
            return rotateLeft(node);
        }
        
        return node;
    }
    
    bool containsNode(Node* node, int key) const {
        if (!node) return false;
        if (key == node->key) return true;
        return key < node->key ? containsNode(node->left, key) : containsNode(node->right, key);
    }
    
    void deleteTree(Node* node) {
        if (node) {
            deleteTree(node->left);
            deleteTree(node->right);
            delete node;
        }
    }

public:
    NewAVLTree() : root(nullptr) {}
    ~NewAVLTree() override { deleteTree(root); }
    
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
