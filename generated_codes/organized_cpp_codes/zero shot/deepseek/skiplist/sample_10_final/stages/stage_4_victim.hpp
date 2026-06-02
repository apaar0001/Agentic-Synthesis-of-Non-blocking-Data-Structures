#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <thread>
#include <chrono>

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
    std::random_device rd;
    mutable std::mt19937 gen;
    std::uniform_real_distribution<> dis;
    
    int randomLevel() {
        int level = 1;
        while (dis(gen) < 0.5 && level < MAX_LEVEL - 1) {
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
                    while (is_marked_ref(succ)) {
                        snip = pred->forward[level].compare_exchange_strong(
                            curr, get_unmarked_ref(succ),
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        if (!snip) goto retry;
                        curr = pred->forward[level].load(std::memory_order_acquire);
                        if (curr == nullptr) {
                            succ = nullptr;
                            break;
                        }
                        succ = curr->forward[level].load(std::memory_order_acquire);
                    }
                    if (curr == nullptr || curr->val >= key) {
                        succ = curr;
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
                preds[level] = pred;
                succs[level] = succ;
            }
            return (succs[0] != nullptr && succs[0]->val == key);
        }
    }
    
public:
    ConcurrentDataStructure() : gen(rd()), dis(0.0, 1.0) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
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
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == nullptr) break;
                succ = curr->forward[level].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    curr = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                    if (curr == nullptr) break;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }
                if (curr == nullptr || curr->val > key) {
                    break;
                }
                if (curr->val == key) {
                    return !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
                }
                pred = curr;
                curr = succ;
            }
        }
        return false;
    }
    
    bool add(int key) override {
        int topLevel = randomLevel();
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                Node* nodeFound = succs[0];
                if (!is_marked_ref(nodeFound->forward[0].load(std::memory_order_acquire))) {
                    return false;
                }
                continue;
            }
            
            Node* newNode = new Node(key, topLevel);
            for (int level = 0; level <= topLevel; ++level) {
                newNode->forward[level].store(succs[level], std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            if (!pred->forward[0].compare_exchange_strong(
                succ, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            
            for (int level = 1; level <= topLevel; ++level) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred->forward[level].compare_exchange_strong(
                        succ, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                            std::this_thread::sleep_for(std::chrono::seconds(5));
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
                    nodeToRemove->forward[level].compare_exchange_strong(
                        succ, get_marked_ref(succ),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                }
            }
            
            succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            while (true) {
                bool marked = is_marked_ref(succ);
                Node* unmarkedSucc = get_unmarked_ref(succ);
                if (nodeToRemove->forward[0].compare_exchange_strong(
                    succ, get_marked_ref(unmarkedSucc),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    break;
                }
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            }
            
            find(key, preds, succs);
            return true;
        }
    }
};