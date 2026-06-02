#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

class ConcurrentDataStructure : public SetADT {
private:
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
    
    static constexpr int MAX_LEVEL = 16;
    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
    
    static bool is_marked_ref(Node* ptr) {
        return ptr && (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
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
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        
    retry:
        pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            while (true) {
                succ = curr->forward[i].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[i].compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                    succ = curr->forward[i].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ);
                } else {
                    break;
                }
            }
            preds[i] = pred;
            succs[i] = curr;
        }
        return succs[0]->val == key;
    }
    
public:
    ConcurrentDataStructure() : rng(std::random_device{}()), dist(0, 1) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
        }
    }
    
    ~ConcurrentDataStructure() override {
        Node* current = head;
        while (current) {
            Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
    }
    
    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            while (true) {
                succ = curr->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    curr = get_unmarked_ref(succ);
                    succ = curr->forward[i].load(std::memory_order_acquire);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ);
                } else {
                    break;
                }
            }
        }
        return curr->val == key;
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
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            Node* nodeToRemove = succs[0];
            
            for (int i = nodeToRemove->topLevel; i >= 1; --i) {
                Node* succ = nullptr;
                while (true) {
                    succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(succ)) {
                        break;
                    }
                    Node* unmarked = get_unmarked_ref(succ);
                    Node* marked = get_marked_ref(unmarked);
                    if (nodeToRemove->forward[i].compare_exchange_strong(succ, marked,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            
            Node* succ = nullptr;
            while (true) {
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
                Node* unmarked = get_unmarked_ref(succ);
                Node* marked = get_marked_ref(unmarked);
                if (nodeToRemove->forward[0].compare_exchange_strong(succ, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    break;
                }
            }
            
            find(key, preds, succs);
            return true;
        }
    }
};