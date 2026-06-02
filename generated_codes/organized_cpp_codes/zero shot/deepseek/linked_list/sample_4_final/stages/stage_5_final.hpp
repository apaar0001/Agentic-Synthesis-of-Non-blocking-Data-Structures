#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
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
    
    std::atomic<Node*> head;
    
public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX, nullptr);
        Node* sentinel = new Node(INT_MIN, tail);
        head.store(sentinel, std::memory_order_relaxed);
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* next = curr->next.load(std::memory_order_relaxed);
            Node* unmarked = get_unmarked_ref(next);
            delete curr;
            curr = unmarked;
        }
    }
    
    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        while (curr && get_unmarked_ref(curr)->val < key) {
            curr = get_unmarked_ref(curr)->next.load(std::memory_order_acquire);
        }
        Node* unmarked = get_unmarked_ref(curr);
        return unmarked && unmarked->val == key && !is_marked_ref(curr);
    }
    
    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(curr)) {
                    Node* unmarked = get_unmarked_ref(curr);
                    if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry_add;
                    }
                    curr = succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (get_unmarked_ref(curr)->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (get_unmarked_ref(curr)->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            
            delete newNode;
            retry_add:;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(curr)) {
                    Node* unmarked = get_unmarked_ref(curr);
                    if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry_remove;
                    }
                    curr = succ;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (get_unmarked_ref(curr)->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (get_unmarked_ref(curr)->val != key) {
                return false;
            }
            
            Node* marked = get_marked_ref(curr);
            if (!curr->next.compare_exchange_strong(succ, marked,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            if (pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete get_unmarked_ref(curr);
            }
            return true;
            
            retry_remove:;
        }
    }
};