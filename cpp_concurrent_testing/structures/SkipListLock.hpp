#pragma once
#include "../utils/SetADT.hpp"
#include <mutex>
#include <climits>
#include <random>
#include <vector>

/*
 * Lock-Based Skip List
 * 
 * Simplified skip list with global locking for thread safety.
 */

class SkipListLock : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    
    struct Node {
        int val;
        int topLevel;
        std::vector<Node*> next;
        
        Node(int v, int level) 
            : val(v), topLevel(level), next(level + 1, nullptr) {}
    };
    
    Node* head;
    std::mt19937 rng;
    std::mutex globalMutex;
    
    int randomLevel() {
        int level = 0;
        while (level < MAX_LEVEL - 1 && (rng() % 2) == 0) {
            level++;
        }
        return level;
    }

public:
    SkipListLock() : rng(std::random_device{}()) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->next[i] = nullptr;
        }
    }
    
    ~SkipListLock() override {
        Node* curr = head;
        while (curr) {
            Node* next = curr->next[0];
            delete curr;
            curr = next;
        }
    }
    
    bool contains(int key) override {
        std::lock_guard<std::mutex> lock(globalMutex);
        
        Node* curr = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            while (curr->next[level] && curr->next[level]->val < key) {
                curr = curr->next[level];
            }
        }
        
        curr = curr->next[0];
        return (curr && curr->val == key);
    }
    
    bool add(int key) override {
        std::lock_guard<std::mutex> lock(globalMutex);
        
        std::vector<Node*> update(MAX_LEVEL);
        Node* curr = head;
        
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            while (curr->next[level] && curr->next[level]->val < key) {
                curr = curr->next[level];
            }
            update[level] = curr;
        }
        
        curr = curr->next[0];
        if (curr && curr->val == key) {
            return false;
        }
        
        int newLevel = randomLevel();
        Node* newNode = new Node(key, newLevel);
        
        for (int level = 0; level <= newLevel; level++) {
            newNode->next[level] = update[level]->next[level];
            update[level]->next[level] = newNode;
        }
        
        return true;
    }
    
    bool remove(int key) override {
        std::lock_guard<std::mutex> lock(globalMutex);
        
        std::vector<Node*> update(MAX_LEVEL);
        Node* curr = head;
        
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            while (curr->next[level] && curr->next[level]->val < key) {
                curr = curr->next[level];
            }
            update[level] = curr;
        }
        
        curr = curr->next[0];
        if (!curr || curr->val != key) {
            return false;
        }
        
        for (int level = 0; level <= curr->topLevel; level++) {
            update[level]->next[level] = curr->next[level];
        }
        
        delete curr;
        return true;
    }
};
