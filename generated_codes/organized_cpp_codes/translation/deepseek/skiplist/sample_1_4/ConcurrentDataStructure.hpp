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
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
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
        
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                    continue;
                }
                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
        }
        
        curr = get_unmarked_ref(curr);
        return curr->val == key && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        std::vector<Node*> preds(MAX_LEVEL);
        std::vector<Node*> succs(MAX_LEVEL);
        
        while (true) {
            Node* pred = head;
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                Node* curr = pred->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (is_marked_ref(curr)) {
                        Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                        pred->forward[i].compare_exchange_strong(curr, succ, 
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        curr = pred->forward[i].load(std::memory_order_acquire);
                        continue;
                    }
                    if (unmarked_curr->val < key) {
                        pred = unmarked_curr;
                        curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                    } else {
                        break;
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            Node* curr = get_unmarked_ref(succs[0]);
            if (curr->val == key) {
                if (is_marked_ref(succs[0])) {
                    continue;
                }
                return false;
            }
            
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            
            for (int i = 0; i <= newLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            
            Node* expected = succs[0];
            if (!preds[0]->forward[0].compare_exchange_strong(expected, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            
            for (int i = 1; i <= newLevel; ++i) {
                while (true) {
                    expected = succs[i];
                    if (preds[i]->forward[i].compare_exchange_strong(expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    
                    pred = preds[i];
                    curr = pred->forward[i].load(std::memory_order_acquire);
                    while (true) {
                        Node* unmarked_curr = get_unmarked_ref(curr);
                        if (is_marked_ref(curr)) {
                            Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                            pred->forward[i].compare_exchange_strong(curr, succ,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                            curr = pred->forward[i].load(std::memory_order_acquire);
                            continue;
                        }
                        if (unmarked_curr->val < key) {
                            pred = unmarked_curr;
                            curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                        } else {
                            break;
                        }
                    }
                    preds[i] = pred;
                    succs[i] = curr;
                    
                    if (get_unmarked_ref(succs[i]) != get_unmarked_ref(newNode->forward[i].load(std::memory_order_relaxed))) {
                        newNode->forward[i].store(succs[i], std::memory_order_relaxed);
                    }
                }
            }
            
            return true;
        }
    }
    
    bool remove(int key) override {
        std::vector<Node*> preds(MAX_LEVEL);
        std::vector<Node*> succs(MAX_LEVEL);
        
        while (true) {
            Node* pred = head;
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                Node* curr = pred->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (is_marked_ref(curr)) {
                        Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                        pred->forward[i].compare_exchange_strong(curr, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        curr = pred->forward[i].load(std::memory_order_acquire);
                        continue;
                    }
                    if (unmarked_curr->val < key) {
                        pred = unmarked_curr;
                        curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                    } else {
                        break;
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            Node* victim = get_unmarked_ref(succs[0]);
            if (victim->val != key) {
                return false;
            }
            
            for (int i = victim->topLevel; i >= 1; --i) {
                Node* expected = victim->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(expected)) {
                    Node* desired = get_marked_ref(get_unmarked_ref(expected));
                    if (victim->forward[i].compare_exchange_strong(expected, desired,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    expected = victim->forward[i].load(std::memory_order_acquire);
                }
            }
            
            Node* expected = victim->forward[0].load(std::memory_order_acquire);
            while (!is_marked_ref(expected)) {
                Node* desired = get_marked_ref(get_unmarked_ref(expected));
                if (victim->forward[0].compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    break;
                }
                expected = victim->forward[0].load(std::memory_order_acquire);
            }
            
            for (int i = victim->topLevel; i >= 0; --i) {
                while (true) {
                    Node* unmarked_succ = get_unmarked_ref(victim->forward[i].load(std::memory_order_acquire));
                    expected = succs[i];
                    if (preds[i]->forward[i].compare_exchange_strong(expected, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    
                    pred = head;
                    for (int j = MAX_LEVEL - 1; j >= i; --j) {
                        Node* curr = pred->forward[j].load(std::memory_order_acquire);
                        while (true) {
                            Node* unmarked_curr = get_unmarked_ref(curr);
                            if (is_marked_ref(curr)) {
                                Node* succ = unmarked_curr->forward[j].load(std::memory_order_acquire);
                                pred->forward[j].compare_exchange_strong(curr, succ,
                                    std::memory_order_acq_rel, std::memory_order_acquire);
                                curr = pred->forward[j].load(std::memory_order_acquire);
                                continue;
                            }
                            if (unmarked_curr->val < key) {
                                pred = unmarked_curr;
                                curr = unmarked_curr->forward[j].load(std::memory_order_acquire);
                            } else {
                                break;
                            }
                        }
                        if (j == i) {
                            preds[i] = pred;
                            succs[i] = curr;
                        }
                    }
                    
                    if (get_unmarked_ref(succs[i]) != victim) {
                        return true;
                    }
                }
            }
            
            return true;
        }
    }
};