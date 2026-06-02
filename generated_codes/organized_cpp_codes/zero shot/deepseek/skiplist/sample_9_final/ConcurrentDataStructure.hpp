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
        
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };
    
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }
    
    Node* head;
    Node* tail;
    std::atomic<int> size;
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
    
    int randomLevel() {
        int level = 1;
        while (distribution(generator) && level < MAX_LEVEL - 1) {
            ++level;
        }
        return level;
    }
    
    bool find(int key, Node** preds, Node** succs) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        bool snip;
        
    retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL - 1; level >= 0; --level) {
                curr = pred->forward[level].load(std::memory_order_acquire);
                while (true) {
                    if (curr == nullptr) {
                        succ = nullptr;
                        break;
                    }
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                    succ = get_unmarked_ref(succ);
                    while (marked) {
                        snip = pred->forward[level].compare_exchange_strong(
                            curr, succ, std::memory_order_acq_rel);
                        if (!snip) goto retry;
                        curr = pred->forward[level].load(std::memory_order_acquire);
                        if (curr == nullptr) {
                            succ = nullptr;
                            break;
                        }
                        succ = curr->forward[level].load(std::memory_order_acquire);
                        marked = is_marked_ref(succ);
                        succ = get_unmarked_ref(succ);
                    }
                    if (curr != nullptr && curr->val < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                if (preds != nullptr) preds[level] = pred;
                if (succs != nullptr) succs[level] = curr;
            }
            return curr != nullptr && curr->val == key;
        }
    }
    
public:
    ConcurrentDataStructure() : size(0), distribution(0, 1) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head;
        Node* next;
        while (curr != nullptr) {
            next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }
    
    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == nullptr) return false;
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                succ = get_unmarked_ref(succ);
                while (marked) {
                    curr = succ;
                    if (curr == nullptr) return false;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                    succ = get_unmarked_ref(succ);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr != nullptr && curr->val == key;
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            int topLevel = randomLevel();
            Node* newNode = new Node(key, topLevel);
            
            for (int level = 0; level <= topLevel; ++level) {
                Node* succ = succs[level];
                newNode->forward[level].store(succ, std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            newNode->forward[0].store(succ, std::memory_order_relaxed);
            
            if (!pred->forward[0].compare_exchange_strong(
                succ, newNode, std::memory_order_acq_rel)) {
                delete newNode;
                continue;
            }
            
            for (int level = 1; level <= topLevel; ++level) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred->forward[level].compare_exchange_strong(
                        succ, newNode, std::memory_order_acq_rel)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            
            size.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    
    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* succ;
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            Node* nodeToRemove = succs[0];
            for (int level = nodeToRemove->topLevel; level >= 1; --level) {
                succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    Node* markedSucc = get_marked_ref(succ);
                    if (nodeToRemove->forward[level].compare_exchange_strong(
                        succ, markedSucc, std::memory_order_acq_rel)) {
                        break;
                    }
                    succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                }
            }
            
            succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            while (true) {
                Node* markedSucc = get_marked_ref(succ);
                if (nodeToRemove->forward[0].compare_exchange_strong(
                    succ, markedSucc, std::memory_order_acq_rel)) {
                    break;
                }
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            }
            
            find(key, preds, succs);
            size.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }
    }
};