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
    std::random_device rd;
    mutable std::mt19937 gen;
    std::uniform_real_distribution<> dis;
    
    int random_level() const {
        int level = 1;
        while (dis(gen) < 0.5 && level < MAX_LEVEL - 1) {
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
                    if (marked) {
                        Node* unmarked_succ = get_unmarked_ref(succ);
                        Node* expected = curr;
                        if (!pred->forward[level].compare_exchange_strong(expected, unmarked_succ,
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            goto retry;
                        }
                        curr = unmarked_succ;
                        if (curr == tail) {
                            succ = nullptr;
                            break;
                        }
                        succ = curr->forward[level].load(std::memory_order_acquire);
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
            return (curr != nullptr && curr->val == key);
        }
    }
    
public:
    ConcurrentDataStructure() : gen(rd()), dis(0.0, 1.0), current_max_level(0) {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        
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
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool marked = false;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                if (curr == nullptr) break;
                succ = curr->forward[level].load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                while (marked) {
                    curr = get_unmarked_ref(succ);
                    if (curr == nullptr) break;
                    succ = curr->forward[level].load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                }
                if (curr == nullptr || curr->val > key) break;
                if (curr->val == key) {
                    return !is_marked_ref(curr->forward[level].load(std::memory_order_acquire));
                }
                pred = curr;
                curr = succ;
            }
        }
        return false;
    }
    
    bool add(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                Node* node_found = succs[0];
                if (!is_marked_ref(node_found->forward[0].load(std::memory_order_acquire))) {
                    return false;
                }
                continue;
            }
            
            int top_level = random_level();
            Node* new_node = new Node(key, top_level);
            
            for (int level = 0; level <= top_level; ++level) {
                Node* succ = succs[level];
                new_node->forward[level].store(succ, std::memory_order_relaxed);
            }
            
            Node* pred = preds[0];
            Node* succ = succs[0];
            
            if (!pred->forward[0].compare_exchange_strong(succ, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete new_node;
                continue;
            }
            
            for (int level = 1; level <= top_level; ++level) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred->forward[level].compare_exchange_strong(succ, new_node,
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
        bool is_marked = false;
        
        while (true) {
            bool found = find(key, preds, succs);
            if (!found) {
                return false;
            }
            
            victim = succs[0];
            if (victim->val != key) {
                return false;
            }
            
            for (int level = victim->topLevel; level >= 1; --level) {
                Node* next = victim->forward[level].load(std::memory_order_acquire);
                while (!is_marked_ref(next)) {
                    Node* marked_next = get_marked_ref(next);
                    if (victim->forward[level].compare_exchange_strong(next, marked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    next = victim->forward[level].load(std::memory_order_acquire);
                }
            }
            
            Node* next = victim->forward[0].load(std::memory_order_acquire);
            while (true) {
                Node* marked_next = get_marked_ref(next);
                if (victim->forward[0].compare_exchange_strong(next, marked_next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    break;
                }
                next = victim->forward[0].load(std::memory_order_acquire);
            }
            
            find(key, preds, succs);
            return true;
        }
    }
};