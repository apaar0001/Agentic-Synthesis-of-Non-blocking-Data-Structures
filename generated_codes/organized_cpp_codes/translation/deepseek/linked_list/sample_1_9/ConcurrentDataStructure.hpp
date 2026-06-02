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
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    void find(int key, Node*& pred, Node*& curr) {
        bool marked;
        Node* succ;
        
        retry:
        pred = head.load(std::memory_order_acquire);
        curr = pred->next.load(std::memory_order_acquire);
        
        while (true) {
            succ = curr->next.load(std::memory_order_acquire);
            marked = is_marked_ref(succ);
            
            while (marked) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (!pred->next.compare_exchange_strong(unmarked_curr, get_unmarked_ref(succ), 
                                                       std::memory_order_acq_rel)) {
                    goto retry;
                }
                curr = get_unmarked_ref(succ);
                if (curr == tail.load(std::memory_order_relaxed)) {
                    return;
                }
                succ = curr->next.load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
            }
            
            if (curr->val >= key) {
                return;
            }
            
            pred = curr;
            curr = get_unmarked_ref(succ);
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
            delete get_unmarked_ref(temp);
        }
    }
    
    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        curr = curr->next.load(std::memory_order_acquire);
        
        while (curr != tail.load(std::memory_order_relaxed)) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (!is_marked_ref(succ)) {
                if (curr->val == key) {
                    return true;
                }
                if (curr->val > key) {
                    return false;
                }
            }
            curr = get_unmarked_ref(succ);
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, pred, curr);
            
            if (curr != tail.load(std::memory_order_relaxed) && curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            Node* unmarked_pred_next = get_unmarked_ref(pred_next);
            if (pred->next.compare_exchange_strong(unmarked_pred_next, newNode, 
                                                  std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, pred, curr);
            
            if (curr == tail.load(std::memory_order_relaxed) || curr->val != key) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!curr->next.compare_exchange_strong(succ, marked_succ, 
                                                   std::memory_order_acq_rel)) {
                continue;
            }
            
            // Node has been marked
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            Node* unmarked_pred_next = get_unmarked_ref(pred_next);
            Node* unmarked_curr = get_unmarked_ref(curr);
            pred->next.compare_exchange_strong(unmarked_curr, succ, 
                                              std::memory_order_acq_rel);
            return true;
        }
    }
};