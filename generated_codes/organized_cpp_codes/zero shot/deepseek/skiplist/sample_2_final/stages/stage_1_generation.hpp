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
        
        Node(int v, int height) : val(v), topLevel(height) {
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
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<> dis;
    
    int randomLevel() {
        int level = 1;
        while (dis(gen) && level < MAX_LEVEL - 1) {
            ++level;
        }
        return level;
    }
    
    void find(int key, Node** preds, Node** succs) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
    retry:
        pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == nullptr) {
                    succ = nullptr;
                    break;
                }
                Node* unmarked_curr = get_unmarked_ref(curr);
                succ = unmarked_curr->forward[level].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    Node* marked_succ = get_marked_ref(unmarked_succ);
                    if (!pred->forward[level].compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = pred->forward[level].load(std::memory_order_acquire);
                    if (curr == nullptr) {
                        succ = nullptr;
                        break;
                    }
                    unmarked_curr = get_unmarked_ref(curr);
                    succ = unmarked_curr->forward[level].load(std::memory_order_acquire);
                }
                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            preds[level] = pred;
            succs[level] = curr;
        }
    }
    
public:
    ConcurrentDataStructure() : gen(rd()), dis(0, 1) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
        }
        size.store(0, std::memory_order_relaxed);
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
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == nullptr) return false;
                Node* unmarked_curr = get_unmarked_ref(curr);
                succ = unmarked_curr->forward[level].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    curr = pred->forward[level].load(std::memory_order_acquire);
                    if (curr == nullptr) return false;
                    unmarked_curr = get_unmarked_ref(curr);
                    succ = unmarked_curr->forward[level].load(std::memory_order_acquire);
                }
                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        Node* unmarked_curr = get_unmarked_ref(curr);
        return unmarked_curr->val == key;
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            find(key, preds, succs);
            
            Node* found = get_unmarked_ref(succs[0]);
            if (found != nullptr && found->val == key) {
                if (is_marked_ref(succs[0])) {
                    continue;
                }
                return false;
            }
            
            int height = randomLevel();
            Node* newNode = new Node(key, height - 1);
            
            for (int level = 0; level < height; ++level) {
                Node* succ = succs[level];
                newNode->forward[level].store(succ, std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            if (!pred->forward[0].compare_exchange_strong(succ, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            
            for (int level = 1; level < height; ++level) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred->forward[level].compare_exchange_strong(succ, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
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
        Node* victim = nullptr;
        
        while (true) {
            find(key, preds, succs);
            
            victim = get_unmarked_ref(succs[0]);
            if (victim == nullptr || victim->val != key) {
                return false;
            }
            
            if (!is_marked_ref(succs[0])) {
                for (int level = victim->topLevel; level >= 0; --level) {
                    Node* next = victim->forward[level].load(std::memory_order_acquire);
                    while (!is_marked_ref(next)) {
                        Node* marked_next = get_marked_ref(get_unmarked_ref(next));
                        if (victim->forward[level].compare_exchange_strong(next, marked_next,
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            break;
                        }
                        next = victim->forward[level].load(std::memory_order_acquire);
                    }
                }
            }
            
            Node* expected = victim;
            Node* desired = get_marked_ref(victim);
            if (preds[0]->forward[0].compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                for (int level = victim->topLevel; level >= 0; --level) {
                    while (true) {
                        Node* pred = preds[level];
                        Node* succ = succs[level];
                        if (pred->forward[level].compare_exchange_strong(succ, 
                                victim->forward[level].load(std::memory_order_acquire),
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            break;
                        }
                        find(key, preds, succs);
                    }
                }
                size.fetch_sub(1, std::memory_order_relaxed);
                return true;
            }
        }
    }
};