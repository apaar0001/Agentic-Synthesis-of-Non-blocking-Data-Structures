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
    std::atomic<int> current_max;
    
    int random_level() {
        static thread_local std::mt19937 generator(std::random_device{}());
        static thread_local std::uniform_int_distribution<int> distribution(0, 1);
        int level = 0;
        while (level < MAX_LEVEL - 1 && distribution(generator) == 0) {
            ++level;
        }
        return level;
    }
    
public:
    ConcurrentDataStructure() {
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        current_max.store(0, std::memory_order_relaxed);
        
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
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
            
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[level].compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
                    succ = curr->forward[level].load(std::memory_order_acquire);
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
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        
        while (true) {
            int lfound = find(key, preds, succs);
            if (lfound != -1) {
                Node* node_found = succs[lfound];
                if (!is_marked_ref(node_found->forward[lfound].load(std::memory_order_acquire))) {
                    return false;
                }
                continue;
            }
            
            int top_level = random_level();
            int old_max = current_max.load(std::memory_order_acquire);
            if (top_level > old_max) {
                current_max.compare_exchange_strong(old_max, top_level,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            
            Node* new_node = new Node(key, top_level);
            for (int i = 0; i <= top_level; ++i) {
                new_node->forward[i].store(succs[i], std::memory_order_relaxed);
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
        Node* succ;
        
        while (true) {
            int lfound = find(key, preds, succs);
            if (lfound == -1) {
                return false;
            }
            
            Node* node_to_remove = succs[lfound];
            for (int level = node_to_remove->topLevel; level >= 1; --level) {
                succ = node_to_remove->forward[level].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    Node* marked_succ = get_marked_ref(get_unmarked_ref(succ));
                    if (node_to_remove->forward[level].compare_exchange_strong(succ, marked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    succ = node_to_remove->forward[level].load(std::memory_order_acquire);
                }
            }
            
            succ = node_to_remove->forward[0].load(std::memory_order_acquire);
            while (true) {
                Node* marked_succ = get_marked_ref(get_unmarked_ref(succ));
                if (node_to_remove->forward[0].compare_exchange_strong(succ, marked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    break;
                }
                succ = node_to_remove->forward[0].load(std::memory_order_acquire);
            }
            
            if (find_and_cleanup(key, preds, succs)) {
                return true;
            }
        }
    }
    
private:
    int find(int key, Node* preds[], Node* succs[]) {
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        int lfound = -1;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
            
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[level].compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }
                
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ);
                } else {
                    break;
                }
            }
            
            preds[level] = pred;
            succs[level] = curr;
            if (curr->val == key) {
                lfound = level;
            }
        }
        return lfound;
    }
    
    bool find_and_cleanup(int key, Node* preds[], Node* succs[]) {
        Node* pred = head;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
            
            while (true) {
                succ = curr->forward[level].load(std::memory_order_acquire);
                
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[level].compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }
                
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ);
                } else {
                    break;
                }
            }
            
            preds[level] = pred;
            succs[level] = curr;
        }
        
        return succs[0]->val == key && 
               is_marked_ref(succs[0]->forward[0].load(std::memory_order_acquire));
    }
};