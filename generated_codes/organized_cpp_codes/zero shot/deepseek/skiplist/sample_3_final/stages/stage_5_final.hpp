#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    
    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];
        
        Node(int key, int height) : val(key), topLevel(height) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    std::atomic<int> currentHeight;
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
    
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }
    
    int randomLevel() {
        int level = 1;
        while (distribution(generator) && level < MAX_LEVEL - 1) {
            ++level;
        }
        return level;
    }
    
    bool find(int key, Node* preds[], Node* succs[]) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        bool snip;
        
    retry:
        pred = head.load(std::memory_order_acquire);
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    snip = pred->forward[level].compare_exchange_strong(
                        curr, unmarked_succ, 
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    if (!snip) goto retry;
                    curr = pred->forward[level].load(std::memory_order_acquire);
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
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
        return curr->val == key;
    }
    
public:
    ConcurrentDataStructure() : distribution(0, 1), currentHeight(0) {
        Node* h = new Node(INT_MIN, MAX_LEVEL - 1);
        Node* t = new Node(INT_MAX, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            h->forward[i].store(t, std::memory_order_relaxed);
        }
        head.store(h, std::memory_order_release);
        tail.store(t, std::memory_order_release);
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_acquire);
        while (curr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }
    
    bool contains(int key) override {
        Node* pred = head.load(std::memory_order_acquire);
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    curr = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr->val == key;
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        int topLevel = randomLevel();
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            Node* newNode = new Node(key, topLevel);
            for (int i = 0; i <= topLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            if (!pred->forward[0].compare_exchange_strong(
                succ, newNode, 
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            
            for (int i = 1; i <= topLevel; ++i) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred->forward[i].compare_exchange_strong(
                        succ, newNode, 
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
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* victim = nullptr;
        bool isMarked = false;
        int topLevel = -1;
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            victim = succs[0];
            if (!isMarked) {
                topLevel = victim->topLevel;
                for (int i = topLevel; i >= 0; --i) {
                    Node* succ = victim->forward[i].load(std::memory_order_acquire);
                    while (!is_marked_ref(succ)) {
                        Node* marked_succ = get_marked_ref(succ);
                        if (victim->forward[i].compare_exchange_strong(
                            succ, marked_succ, 
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            break;
                        }
                        succ = victim->forward[i].load(std::memory_order_acquire);
                    }
                }
                isMarked = true;
            }
            
            Node* expected = victim;
            Node* desired = get_marked_ref(victim->forward[0].load(std::memory_order_acquire));
            if (preds[0]->forward[0].compare_exchange_strong(
                expected, desired, 
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                find(key, preds, succs);
                return true;
            }
        }
    }
};