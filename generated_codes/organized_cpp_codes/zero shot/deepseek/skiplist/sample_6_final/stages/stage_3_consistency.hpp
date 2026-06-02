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
    std::atomic<int> current_max_level;
    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution;
    
    int random_level() {
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
                        if (!snip) {
                            goto retry;
                        }
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
            return (curr != nullptr && curr->val == key);
        }
    }
    
public:
    ConcurrentDataStructure() : 
        distribution(0, 1), 
        current_max_level(0) {
        
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
                if (curr == nullptr) {
                    succ = nullptr;
                    break;
                }
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                succ = get_unmarked_ref(succ);
                
                while (marked) {
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
        }
        return (curr != nullptr && curr->val == key);
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;
            }
            
            int level = random_level();
            Node* new_node = new Node(key, level);
            
            for (int i = 0; i <= level; ++i) {
                Node* succ = succs[i];
                new_node->forward[i].store(succ, std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            new_node->forward[0].store(succ, std::memory_order_relaxed);
            if (!pred->forward[0].compare_exchange_strong(
                succ, new_node, std::memory_order_acq_rel)) {
                delete new_node;
                continue;
            }
            
            for (int i = 1; i <= level; ++i) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred->forward[i].compare_exchange_strong(
                        succ, new_node, std::memory_order_acq_rel)) {
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
        Node* node_to_remove = nullptr;
        bool is_marked = false;
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            node_to_remove = succs[0];
            for (int level = node_to_remove->topLevel; level >= 1; --level) {
                Node* succ = node_to_remove->forward[level].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (node_to_remove->forward[level].compare_exchange_strong(
                        succ, marked_succ, std::memory_order_acq_rel)) {
                        break;
                    }
                    succ = node_to_remove->forward[level].load(std::memory_order_acquire);
                }
            }
            
            Node* succ = node_to_remove->forward[0].load(std::memory_order_acquire);
            while (true) {
                Node* marked_succ = get_marked_ref(succ);
                bool i_marked_it = node_to_remove->forward[0].compare_exchange_strong(
                    succ, marked_succ, std::memory_order_acq_rel);
                succ = succs[0]->forward[0].load(std::memory_order_acquire);
                if (i_marked_it) {
                    find(key, preds, succs);
                    return true;
                } else if (is_marked_ref(succ)) {
                    return false;
                }
            }
        }
    }
};