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
        std::vector<std::atomic<Node*>> forward;
        
        Node(int value, int level) : val(value), topLevel(level), forward(level + 1) {
            for (auto& f : forward) {
                f.store(nullptr, std::memory_order_relaxed);
            }
        }
    };
    
    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    int randomLevel() {
        int level = 0;
        while (dist(rng) && level < MAX_LEVEL - 1) {
            ++level;
        }
        return level;
    }
    
    bool find(int key, std::vector<Node*>& preds, std::vector<Node*>& succs) {
        Node* pred;
        Node* curr;
        Node* succ;
        bool marked;
        
    retry:
        pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[level].compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = unmarked_succ;
                    if (curr == tail) {
                        succ = tail;
                        marked = false;
                    } else {
                        succ = curr->forward[level].load(std::memory_order_acquire);
                        marked = is_marked_ref(succ);
                    }
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return (curr != tail) && (curr->val == key);
    }
    
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL - 1)), 
                                tail(new Node(INT_MAX, MAX_LEVEL - 1)),
                                rng(std::random_device{}()),
                                dist(0, 1) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
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
        Node* succ;
        bool marked;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = head;
            succ = curr->forward[level].load(std::memory_order_acquire);
            while (true) {
                marked = is_marked_ref(succ);
                while (marked) {
                    curr = get_unmarked_ref(succ);
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (succ->val < key) {
                    curr = succ;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
        }
        
        curr = succ;
        return (curr != tail) && (curr->val == key) && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        std::vector<Node*> preds(MAX_LEVEL);
        std::vector<Node*> succs(MAX_LEVEL);
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            
            for (int i = 0; i <= newLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            if (!pred->forward[0].compare_exchange_strong(succ, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            
            for (int i = 1; i <= newLevel; ++i) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred->forward[i].compare_exchange_strong(succ, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            
            return true;
        }
    }
    
    bool remove(int key) override {
        std::vector<Node*> preds(MAX_LEVEL);
        std::vector<Node*> succs(MAX_LEVEL);
        Node* succ;
        bool marked;
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            Node* nodeToRemove = succs[0];
            
            for (int i = nodeToRemove->topLevel; i >= 1; --i) {
                succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (!marked) {
                    if (nodeToRemove->forward[i].compare_exchange_strong(succ, get_marked_ref(succ),
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
            }
            
            succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            marked = is_marked_ref(succ);
            while (!marked) {
                if (nodeToRemove->forward[0].compare_exchange_strong(succ, get_marked_ref(succ),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    find(key, preds, succs);
                    return true;
                }
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
            }
            return false;
        }
    }
};