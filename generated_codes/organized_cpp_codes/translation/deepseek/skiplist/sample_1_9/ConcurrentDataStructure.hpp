#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    
    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*>* forward;
        
        Node(int value, int level) : val(value), topLevel(level) {
            forward = new std::atomic<Node*>[level + 1];
            for (int i = 0; i <= level; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
        
        ~Node() {
            delete[] forward;
        }
    };
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1)) != 0;
    }
    
    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
    
    int randomLevel() {
        int level = 0;
        while (dist(rng) && level < MAX_LEVEL - 1) {
            ++level;
        }
        return level;
    }
    
    bool find(int key, Node** preds, Node** succs) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        bool snip = false;
        
        retry:
        pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == tail) break;
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    snip = pred->forward[level].compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel);
                    if (!snip) {
                        goto retry;
                    }
                    curr = unmarked_succ;
                    if (curr == tail) break;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (curr == tail || curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return curr != tail && curr->val == key;
    }
    
public:
    ConcurrentDataStructure() : rng(std::random_device{}()), dist(0, 1) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }
    
    bool contains(int key) override {
        Node* curr = head;
        Node* succ = nullptr;
        bool marked = false;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = head->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == tail) break;
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    curr = get_unmarked_ref(succ);
                    if (curr == tail) break;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (curr == tail || curr->val >= key) {
                    break;
                }
                curr = succ;
            }
        }
        return curr != tail && curr->val == key;
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            
            for (int level = 0; level <= newLevel; ++level) {
                newNode->forward[level].store(succs[level], std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            if (!pred->forward[0].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel)) {
                delete newNode;
                continue;
            }
            
            for (int level = 1; level <= newLevel; ++level) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred->forward[level].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }
    
    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            Node* nodeToDelete = succs[0];
            for (int level = nodeToDelete->topLevel; level >= 1; --level) {
                Node* succ = nodeToDelete->forward[level].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (nodeToDelete->forward[level].compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                        break;
                    }
                    succ = nodeToDelete->forward[level].load(std::memory_order_acquire);
                }
            }
            
            Node* succ = nodeToDelete->forward[0].load(std::memory_order_acquire);
            while (!is_marked_ref(succ)) {
                Node* marked_succ = get_marked_ref(succ);
                if (nodeToDelete->forward[0].compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                    // Node has been marked
                    break;
                }
                succ = nodeToDelete->forward[0].load(std::memory_order_acquire);
            }
            
            find(key, preds, succs);
            return true;
        }
    }
};