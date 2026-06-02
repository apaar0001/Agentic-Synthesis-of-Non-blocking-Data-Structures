#pragma once

#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>

/**
 * TreeLock - Lock-based Binary Search Tree (Citrus algorithm)
 * 
 * Based on the Citrus tree by Maya Arbel (2014)
 * Uses optimistic traversal with validation and per-node locking
 * 
 * Algorithm:
 * - Optimistic search without locks (read-only traversal)
 * - Validation before modification (check marked flag and parent-child relationship)
 * - Per-node locks acquired only during modification
 * - Tag counters track null child changes for validation
 */
class TreeLock : public SetADT {
private:
    struct Node {
        int key;
        Node* child[2];           // child[0] = left, child[1] = right
        std::mutex lock;
        bool marked;              // Logical deletion flag
        int tag[2];               // Version counters for null children
        
        Node(int k) : key(k), marked(false) {
            child[0] = nullptr;
            child[1] = nullptr;
            tag[0] = 0;
            tag[1] = 0;
        }
    };

    Node* root;

    // Validate that the parent-child relationship is still valid
    bool validate(Node* prev, int prevTag, Node* curr, int direction) {
        if (curr == nullptr) {
            // For null children, check marked flag, child pointer, and tag
            return (!prev->marked && 
                    prev->child[direction] == curr && 
                    prev->tag[direction] == prevTag);
        } else {
            // For non-null children, check marked flags and child pointer
            return (!prev->marked && 
                    !curr->marked && 
                    prev->child[direction] == curr);
        }
    }

    // Recursive helper to delete all nodes in the tree
    void deleteTree(Node* node) {
        if (node == nullptr) return;
        deleteTree(node->child[0]);
        deleteTree(node->child[1]);
        delete node;
    }

public:
    TreeLock() {
        // Initialize with root pointing to a sentinel node with max value
        root = new Node(INT_MAX);
        root->child[0] = new Node(INT_MAX);
    }

    ~TreeLock() override {
        deleteTree(root);
    }

    bool contains(int key) override {
        Node* curr = root->child[0];
        int ckey = curr->key;
        
        while (curr != nullptr && ckey != key) {
            if (ckey > key)
                curr = curr->child[0];
            else if (ckey < key)
                curr = curr->child[1];
            
            if (curr != nullptr)
                ckey = curr->key;
        }
        
        return (curr != nullptr);
    }

    bool add(int key) override {
        while (true) {
            // Optimistic traversal to find insertion point
            Node* prev = root;
            Node* curr = root->child[0];
            int direction = 0;
            int ckey = curr->key;
            int prevTag;
            
            while (curr != nullptr && ckey != key) {
                prev = curr;
                if (ckey > key) {
                    curr = curr->child[0];
                    direction = 0;
                } else if (ckey < key) {
                    curr = curr->child[1];
                    direction = 1;
                }
                
                if (curr != nullptr)
                    ckey = curr->key;
            }
            
            prevTag = prev->tag[direction];
            
            // Key already exists
            if (curr != nullptr)
                return false;
            
            // Lock parent and validate
            std::lock_guard<std::mutex> guard(prev->lock);
            
            if (validate(prev, prevTag, curr, direction)) {
                Node* newNode = new Node(key);
                prev->child[direction] = newNode;
                return true;
            }
            // Validation failed, retry
        }
    }

    bool remove(int key) override {
        while (true) {
            // Optimistic traversal to find node to delete
            Node* prev = root;
            Node* curr = root->child[0];
            int direction = 0;
            int ckey = curr->key;
            
            while (curr != nullptr && ckey != key) {
                prev = curr;
                if (ckey > key) {
                    curr = curr->child[0];
                    direction = 0;
                } else if (ckey < key) {
                    curr = curr->child[1];
                    direction = 1;
                }
                
                if (curr != nullptr)
                    ckey = curr->key;
            }
            
            // Key not found
            if (curr == nullptr)
                return false;
            
            // Lock parent and current node
            std::lock_guard<std::mutex> prevGuard(prev->lock);
            std::lock_guard<std::mutex> currGuard(curr->lock);
            
            // Validate the relationship
            if (!validate(prev, 0, curr, direction)) {
                continue; // Retry
            }
            
            // Case 1: Node has no left child
            if (curr->child[0] == nullptr) {
                curr->marked = true;
                prev->child[direction] = curr->child[1];
                if (prev->child[direction] == nullptr) {
                    prev->tag[direction]++;
                }
                return true;
            }
            
            // Case 2: Node has no right child
            if (curr->child[1] == nullptr) {
                curr->marked = true;
                prev->child[direction] = curr->child[0];
                if (prev->child[direction] == nullptr) {
                    prev->tag[direction]++;
                }
                return true;
            }
            
            // Case 3: Node has two children - find successor
            Node* prevSucc = curr;
            Node* succ = curr->child[1];
            Node* next = succ->child[0];
            
            while (next != nullptr) {
                prevSucc = succ;
                succ = next;
                next = next->child[0];
            }
            
            int succDirection = 1;
            if (prevSucc != curr) {
                succDirection = 0;
            }
            
            // Lock successor's parent if different from curr
            std::unique_lock<std::mutex> prevSuccGuard;
            if (prevSucc != curr) {
                prevSuccGuard = std::unique_lock<std::mutex>(prevSucc->lock);
            }
            
            std::lock_guard<std::mutex> succGuard(succ->lock);
            
            // Validate successor relationships
            if (validate(prevSucc, 0, succ, succDirection) && 
                validate(succ, succ->tag[0], nullptr, 0)) {
                
                curr->marked = true;
                
                // Create replacement node with successor's key
                Node* newNode = new Node(succ->key);
                newNode->child[0] = curr->child[0];
                newNode->child[1] = curr->child[1];
                
                std::lock_guard<std::mutex> newGuard(newNode->lock);
                prev->child[direction] = newNode;
                
                if (prev->child[direction] == nullptr) {
                    prev->tag[direction]++;
                }
                
                succ->marked = true;
                
                // Update successor's parent
                if (prevSucc == curr) {
                    newNode->child[1] = succ->child[1];
                    if (newNode->child[1] == nullptr) {
                        newNode->tag[1]++;
                    }
                } else {
                    prevSucc->child[0] = succ->child[1];
                    if (prevSucc->child[0] == nullptr) {
                        prevSucc->tag[0]++;
                    }
                }
                
                return true;
            }
            // Validation failed, retry
        }
    }
};
