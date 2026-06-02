#pragma once

#include "../utils/SetADT.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>

/**
 * Sequential Skip List implementation
 * 
 * A probabilistic data structure that maintains a sorted set of elements
 * with O(log n) expected time for search, insert, and delete operations.
 * This is a non-concurrent implementation suitable for single-threaded use.
 * 
 * Based on the synchrobench C implementation at:
 * synchrobench/c-cpp/src/skiplists/sequential/
 */
class SequentialSkipList : public SetADT {
private:
    static constexpr int MAX_LEVEL = 32;
    
    struct Node {
        int val;
        int toplevel;
        Node** next;  // Array of next pointers
        
        Node(int v, int level) : val(v), toplevel(level) {
            next = new Node*[level];
            for (int i = 0; i < level; i++) {
                next[i] = nullptr;
            }
        }
        
        ~Node() {
            delete[] next;
        }
    };
    
    Node* head;
    int levelmax;
    
    /**
     * Generate a random level for a new node using xorshift RNG.
     * Results are hardwired to p=0.5, min=1, max=levelmax.
     */
    int getRandomLevel() {
        static uint32_t y = 2463534242UL;
        y ^= (y << 13);
        y ^= (y >> 17);
        y ^= (y << 5);
        uint32_t temp = y;
        uint32_t level = 1;
        while (((temp >>= 1) & 1) != 0) {
            ++level;
        }
        // Clamp to [1, levelmax]
        if (level > static_cast<uint32_t>(levelmax)) {
            return levelmax;
        } else {
            return static_cast<int>(level);
        }
    }
    
public:
    SequentialSkipList() : levelmax(MAX_LEVEL) {
        // Create sentinel nodes: head (INT_MIN) and tail (INT_MAX)
        Node* tail = new Node(INT_MAX, levelmax);
        for (int i = 0; i < levelmax; i++) {
            tail->next[i] = nullptr;
        }
        
        head = new Node(INT_MIN, levelmax);
        for (int i = 0; i < levelmax; i++) {
            head->next[i] = tail;
        }
    }
    
    ~SequentialSkipList() override {
        // Traverse and delete all nodes
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = curr->next[0];
            delete curr;
            curr = next;
        }
    }
    
    /**
     * Check if the skip list contains the given key.
     * Traverses from top level to bottom, moving right when possible.
     */
    bool contains(int key) override {
        Node* node = head;
        
        // Start from the top level and work down
        for (int i = node->toplevel - 1; i >= 0; i--) {
            Node* next = node->next[i];
            // Move right while next value is less than key
            while (next->val < key) {
                node = next;
                next = node->next[i];
            }
        }
        
        // Check the bottom level
        node = node->next[0];
        return (node->val == key);
    }
    
    /**
     * Add a key to the skip list.
     * Returns true if the key was added, false if it already exists.
     */
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* node = head;
        
        // Find predecessors and successors at each level
        for (int i = node->toplevel - 1; i >= 0; i--) {
            Node* next = node->next[i];
            while (next->val < key) {
                node = next;
                next = node->next[i];
            }
            preds[i] = node;
            succs[i] = node->next[i];
        }
        
        // Check if key already exists
        node = node->next[0];
        if (node->val == key) {
            return false;  // Key already exists
        }
        
        // Insert new node
        int level = getRandomLevel();
        Node* newNode = new Node(key, level);
        
        // Link new node at each level
        for (int i = 0; i < level; i++) {
            newNode->next[i] = succs[i];
            preds[i]->next[i] = newNode;
        }
        
        return true;
    }
    
    /**
     * Remove a key from the skip list.
     * Returns true if the key was removed, false if it was not found.
     */
    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* node = head;
        
        // Find predecessors and successors at each level
        for (int i = node->toplevel - 1; i >= 0; i--) {
            Node* next = node->next[i];
            while (next->val < key) {
                node = next;
                next = node->next[i];
            }
            preds[i] = node;
            succs[i] = node->next[i];
        }
        
        // Check if key exists
        Node* target = succs[0];
        if (target->val != key) {
            return false;  // Key not found
        }
        
        // Unlink node at each level
        for (int i = 0; i < head->toplevel; i++) {
            if (succs[i]->val == key) {
                preds[i]->next[i] = succs[i]->next[i];
            }
        }
        
        // Delete the node
        delete target;
        
        return true;
    }
};
