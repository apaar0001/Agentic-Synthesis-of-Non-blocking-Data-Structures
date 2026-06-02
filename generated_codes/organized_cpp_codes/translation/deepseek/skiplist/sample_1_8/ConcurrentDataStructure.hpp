#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;
    
    struct Node {
        int val;
        int topLevel;
        std::vector<std::atomic<Node*>> forward;
        
        Node(int value, int level) : val(value), topLevel(level), forward(level + 1) {
            for (auto& f : forward) {
                f.store(nullptr);
            }
        }
    };
    
    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::uniform_int_distribution<int> dist;
    
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    int randomLevel() {
        int level = 1;
        while (dist(rng) && level < MAX_LEVEL) {
            ++level;
        }
        return level;
    }
    
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)), tail(new Node(INT_MAX, MAX_LEVEL)), rng(std::random_device{}()), dist(0, 1) {
        for (int i = 0; i <= MAX_LEVEL; ++i) {
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
        
        for (int i = MAX_LEVEL; i >= 0; --i) {
            curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                    continue;
                }
                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = pred->forward[i].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
        }
        
        curr = get_unmarked_ref(curr);
        return curr->val == key && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        std::vector<Node*> preds(MAX_LEVEL + 1);
        std::vector<Node*> succs(MAX_LEVEL + 1);
        
        while (true) {
            Node* pred = head;
            for (int i = MAX_LEVEL; i >= 0; --i) {
                Node* curr = pred->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (is_marked_ref(curr)) {
                        Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                        pred->forward[i].compare_exchange_strong(curr, succ, std::memory_order_acq_rel);
                        curr = pred->forward[i].load(std::memory_order_acquire);
                    } else {
                        if (unmarked_curr->val < key) {
                            pred = unmarked_curr;
                            curr = pred->forward[i].load(std::memory_order_acquire);
                        } else {
                            break;
                        }
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            Node* curr = get_unmarked_ref(succs[0]);
            if (curr->val == key) {
                if (is_marked_ref(curr->forward[0].load(std::memory_order_acquire))) {
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
            if (!preds[0]->forward[0].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                delete newNode;
                continue;
            }
            
            for (int i = 1; i <= newLevel; ++i) {
                while (true) {
                    expected = succs[i];
                    if (preds[i]->forward[i].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                        break;
                    }
                    
                    pred = preds[i];
                    curr = pred->forward[i].load(std::memory_order_acquire);
                    while (true) {
                        Node* unmarked_curr = get_unmarked_ref(curr);
                        if (is_marked_ref(curr)) {
                            Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                            pred->forward[i].compare_exchange_strong(curr, succ, std::memory_order_acq_rel);
                            curr = pred->forward[i].load(std::memory_order_acquire);
                        } else {
                            if (unmarked_curr->val < key) {
                                pred = unmarked_curr;
                                curr = pred->forward[i].load(std::memory_order_acquire);
                            } else {
                                break;
                            }
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
        std::vector<Node*> preds(MAX_LEVEL + 1);
        std::vector<Node*> succs(MAX_LEVEL + 1);
        Node* node_to_delete = nullptr;
        
        while (true) {
            Node* pred = head;
            for (int i = MAX_LEVEL; i >= 0; --i) {
                Node* curr = pred->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (is_marked_ref(curr)) {
                        Node* succ = unmarked_curr->forward[i].load(std::memory_order_acquire);
                        pred->forward[i].compare_exchange_strong(curr, succ, std::memory_order_acq_rel);
                        curr = pred->forward[i].load(std::memory_order_acquire);
                    } else {
                        if (unmarked_curr->val < key) {
                            pred = unmarked_curr;
                            curr = pred->forward[i].load(std::memory_order_acquire);
                        } else {
                            break;
                        }
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            
            node_to_delete = get_unmarked_ref(succs[0]);
            if (node_to_delete->val != key) {
                return false;
            }
            
            for (int i = node_to_delete->topLevel; i >= 1; --i) {
                Node* next = node_to_delete->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(next)) {
                    node_to_delete->forward[i].compare_exchange_weak(next, get_marked_ref(next), std::memory_order_acq_rel);
                    next = node_to_delete->forward[i].load(std::memory_order_acquire);
                }
            }
            
            Node* next = node_to_delete->forward[0].load(std::memory_order_acquire);
            while (!is_marked_ref(next)) {
                if (node_to_delete->forward[0].compare_exchange_weak(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    // Node has been marked
                    break;
                }
                next = node_to_delete->forward[0].load(std::memory_order_acquire);
            }
            
            for (int i = node_to_delete->topLevel; i >= 0; --i) {
                while (true) {
                    Node* expected = succs[i];
                    Node* actual = node_to_delete->forward[i].load(std::memory_order_acquire);
                    if (preds[i]->forward[i].compare_exchange_strong(expected, get_unmarked_ref(actual), std::memory_order_acq_rel)) {
                        break;
                    }
                    
                    pred = head;
                    Node* curr = nullptr;
                    for (int j = MAX_LEVEL; j >= i; --j) {
                        curr = pred->forward[j].load(std::memory_order_acquire);
                        while (true) {
                            Node* unmarked_curr = get_unmarked_ref(curr);
                            if (is_marked_ref(curr)) {
                                Node* succ = unmarked_curr->forward[j].load(std::memory_order_acquire);
                                pred->forward[j].compare_exchange_strong(curr, succ, std::memory_order_acq_rel);
                                curr = pred->forward[j].load(std::memory_order_acquire);
                            } else {
                                if (unmarked_curr->val < key) {
                                    pred = unmarked_curr;
                                    curr = pred->forward[j].load(std::memory_order_acquire);
                                } else {
                                    break;
                                }
                            }
                        }
                        preds[j] = pred;
                        succs[j] = curr;
                    }
                    
                    if (get_unmarked_ref(succs[i]) != node_to_delete) {
                        return true;
                    }
                }
            }
            
            return true;
        }
    }
};