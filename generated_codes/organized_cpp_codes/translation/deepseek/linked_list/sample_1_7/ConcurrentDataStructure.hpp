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
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    
    void find(int key, Node* &pred, Node* &curr, Node* &succ) {
        retry:
        pred = head.load(std::memory_order_acquire);
        curr = pred->next.load(std::memory_order_acquire);
        
        while (true) {
            if (is_marked_ref(curr)) {
                curr = get_unmarked_ref(curr);
            }
            
            if (curr == tail.load(std::memory_order_acquire)) {
                succ = nullptr;
                return;
            }
            
            succ = curr->next.load(std::memory_order_acquire);
            
            if (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = unmarked_succ;
                continue;
            }
            
            if (curr->val >= key) {
                return;
            }
            
            pred = curr;
            curr = succ;
        }
    }
    
public:
    ConcurrentDataStructure() {
        Node* t = new Node(INT_MAX);
        t->next.store(nullptr, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
        
        Node* h = new Node(INT_MIN);
        h->next.store(t, std::memory_order_relaxed);
        head.store(h, std::memory_order_relaxed);
    }
    
    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* temp = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete temp;
        }
    }
    
    bool contains(int key) override {
        Node* pred;
        Node* curr;
        Node* succ;
        find(key, pred, curr, succ);
        
        return curr != tail.load(std::memory_order_acquire) && 
               curr->val == key && 
               !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            Node* succ;
            find(key, pred, curr, succ);
            
            if (curr != tail.load(std::memory_order_acquire) && curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            if (is_marked_ref(pred_next)) {
                continue;
            }
            
            if (pred->next.compare_exchange_strong(curr, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            Node* succ;
            find(key, pred, curr, succ);
            
            if (curr == tail.load(std::memory_order_acquire) || curr->val != key) {
                return false;
            }
            
            Node* curr_next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(curr_next)) {
                return false;
            }
            
            Node* marked_next = get_marked_ref(curr_next);
            if (!curr->next.compare_exchange_strong(curr_next, marked_next,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            if (is_marked_ref(pred_next)) {
                continue;
            }
            
            pred->next.compare_exchange_strong(curr, curr_next,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};