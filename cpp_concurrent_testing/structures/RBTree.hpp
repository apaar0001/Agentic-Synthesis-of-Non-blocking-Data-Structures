#pragma once
#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>

/*
 * Red-Black Tree - Lock-based self-balancing BST
 * 
 * Properties:
 * 1. Every node is either red or black
 * 2. Root is black
 * 3. All leaves (NULL) are black
 * 4. Red nodes have black children
 * 5. All paths from root to leaves have same number of black nodes
 */

class RBTree : public SetADT {
private:
    enum Color { RED, BLACK };
    
    struct Node {
        int key;
        Node* left;
        Node* right;
        Node* parent;
        Color color;
        
        Node(int k) : key(k), left(nullptr), right(nullptr), parent(nullptr), color(RED) {}
    };
    
    Node* root;
    std::mutex treeMutex;
    
    void rotateLeft(Node* x) {
        Node* y = x->right;
        x->right = y->left;
        if (y->left) y->left->parent = x;
        y->parent = x->parent;
        if (!x->parent) root = y;
        else if (x == x->parent->left) x->parent->left = y;
        else x->parent->right = y;
        y->left = x;
        x->parent = y;
    }
    
    void rotateRight(Node* x) {
        Node* y = x->left;
        x->left = y->right;
        if (y->right) y->right->parent = x;
        y->parent = x->parent;
        if (!x->parent) root = y;
        else if (x == x->parent->right) x->parent->right = y;
        else x->parent->left = y;
        y->right = x;
        x->parent = y;
    }
    
    void fixInsert(Node* k) {
        while (k->parent && k->parent->color == RED) {
            if (k->parent == k->parent->parent->left) {
                Node* u = k->parent->parent->right;
                if (u && u->color == RED) {
                    k->parent->color = BLACK;
                    u->color = BLACK;
                    k->parent->parent->color = RED;
                    k = k->parent->parent;
                } else {
                    if (k == k->parent->right) {
                        k = k->parent;
                        rotateLeft(k);
                    }
                    k->parent->color = BLACK;
                    k->parent->parent->color = RED;
                    rotateRight(k->parent->parent);
                }
            } else {
                Node* u = k->parent->parent->left;
                if (u && u->color == RED) {
                    k->parent->color = BLACK;
                    u->color = BLACK;
                    k->parent->parent->color = RED;
                    k = k->parent->parent;
                } else {
                    if (k == k->parent->left) {
                        k = k->parent;
                        rotateRight(k);
                    }
                    k->parent->color = BLACK;
                    k->parent->parent->color = RED;
                    rotateLeft(k->parent->parent);
                }
            }
            if (k == root) break;
        }
        root->color = BLACK;
    }
    
    Node* insertNode(Node* node, Node* parent, int key, bool& inserted) {
        if (!node) {
            inserted = true;
            Node* newNode = new Node(key);
            newNode->parent = parent;
            return newNode;
        }
        
        if (key < node->key) {
            node->left = insertNode(node->left, node, key, inserted);
        } else if (key > node->key) {
            node->right = insertNode(node->right, node, key, inserted);
        } else {
            inserted = false;
        }
        
        return node;
    }
    
    Node* minValueNode(Node* node) {
        while (node->left) node = node->left;
        return node;
    }
    
    void transplant(Node* u, Node* v) {
        if (!u->parent) root = v;
        else if (u == u->parent->left) u->parent->left = v;
        else u->parent->right = v;
        if (v) v->parent = u->parent;
    }
    
    void fixDelete(Node* x, Node* xParent) {
        while (x != root && (!x || x->color == BLACK)) {
            if (x == xParent->left) {
                Node* w = xParent->right;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    xParent->color = RED;
                    rotateLeft(xParent);
                    w = xParent->right;
                }
                if (w && (!w->left || w->left->color == BLACK) && 
                    (!w->right || w->right->color == BLACK)) {
                    w->color = RED;
                    x = xParent;
                    xParent = x->parent;
                } else {
                    if (w && (!w->right || w->right->color == BLACK)) {
                        if (w->left) w->left->color = BLACK;
                        w->color = RED;
                        rotateRight(w);
                        w = xParent->right;
                    }
                    if (w) {
                        w->color = xParent->color;
                        xParent->color = BLACK;
                        if (w->right) w->right->color = BLACK;
                        rotateLeft(xParent);
                    }
                    x = root;
                }
            } else {
                Node* w = xParent->left;
                if (w && w->color == RED) {
                    w->color = BLACK;
                    xParent->color = RED;
                    rotateRight(xParent);
                    w = xParent->left;
                }
                if (w && (!w->right || w->right->color == BLACK) && 
                    (!w->left || w->left->color == BLACK)) {
                    w->color = RED;
                    x = xParent;
                    xParent = x->parent;
                } else {
                    if (w && (!w->left || w->left->color == BLACK)) {
                        if (w->right) w->right->color = BLACK;
                        w->color = RED;
                        rotateLeft(w);
                        w = xParent->left;
                    }
                    if (w) {
                        w->color = xParent->color;
                        xParent->color = BLACK;
                        if (w->left) w->left->color = BLACK;
                        rotateRight(xParent);
                    }
                    x = root;
                }
            }
        }
        if (x) x->color = BLACK;
    }
    
    bool removeNode(int key) {
        Node* z = root;
        while (z) {
            if (key < z->key) z = z->left;
            else if (key > z->key) z = z->right;
            else break;
        }
        
        if (!z) return false;
        
        Node* y = z;
        Node* x;
        Node* xParent;
        Color yOriginalColor = y->color;
        
        if (!z->left) {
            x = z->right;
            xParent = z->parent;
            transplant(z, z->right);
        } else if (!z->right) {
            x = z->left;
            xParent = z->parent;
            transplant(z, z->left);
        } else {
            y = minValueNode(z->right);
            yOriginalColor = y->color;
            x = y->right;
            xParent = y;
            if (y->parent == z) {
                if (x) x->parent = y;
            } else {
                xParent = y->parent;
                transplant(y, y->right);
                y->right = z->right;
                y->right->parent = y;
            }
            transplant(z, y);
            y->left = z->left;
            y->left->parent = y;
            y->color = z->color;
        }
        
        delete z;
        
        if (yOriginalColor == BLACK) {
            fixDelete(x, xParent);
        }
        
        return true;
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
    RBTree() : root(nullptr) {}
    ~RBTree() override { deleteTree(root); }
    
    bool contains(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return containsNode(root, key);
    }
    
    bool add(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        bool inserted = false;
        root = insertNode(root, nullptr, key, inserted);
        if (inserted) {
            Node* newNode = root;
            while (newNode && newNode->key != key) {
                newNode = (key < newNode->key) ? newNode->left : newNode->right;
            }
            if (newNode) fixInsert(newNode);
        }
        return inserted;
    }
    
    bool remove(int key) override {
        std::lock_guard<std::mutex> lock(treeMutex);
        return removeNode(key);
    }
};
