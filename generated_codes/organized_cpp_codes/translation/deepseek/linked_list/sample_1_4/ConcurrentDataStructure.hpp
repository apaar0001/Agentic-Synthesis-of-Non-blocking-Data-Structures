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
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    void initialize() {
        Node* h = new Node(INT_MIN);
        Node* t = new Node(INT_MAX);
        h->next.store(t, std::memory_order_relaxed);
        head.store(h, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
    }
    
    void cleanup() {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* temp = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete temp;
        }
    }
    
public:
    ConcurrentDataStructure() {
        initialize();
    }
    
    ~ConcurrentDataStructure() override {
        cleanup();
    }
    
    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire);
        curr = curr->next.load(std::memory_order_acquire);
        
        while (true) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                return false;
            }
            
            if (is_marked_ref(curr)) {
                curr = unmarked_curr->next.load(std::memory_order_acquire);
                continue;
            }
            
            if (unmarked_curr->val == key) {
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                return !is_marked_ref(succ);
            }
            
            if (unmarked_curr->val > key) {
                return false;
            }
            
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
    }
    
    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                    break;
                }
                
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(curr)) {
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (!unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ), std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (unmarked_curr->val >= key) {
                    break;
                }
                
                pred = unmarked_curr;
                curr = succ;
            }
            
            Node* unmarked_pred = get_unmarked_ref(pred);
            Node* unmarked_curr = get_unmarked_ref(curr);
            
            if (unmarked_curr != tail.load(std::memory_order_acquire) && unmarked_curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key, unmarked_curr);
            if (unmarked_pred->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            
            delete newNode;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == tail.load(std::memory_order_acquire)) {
                    return false;
                }
                
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(curr)) {
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (!unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ), std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                if (unmarked_curr->val > key) {
                    return false;
                }
                
                if (unmarked_curr->val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (unmarked_curr->next.compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                        // Node has been marked
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ), std::memory_order_acq_rel);
                        return true;
                    }
                    continue;
                }
                
                pred = unmarked_curr;
                curr = succ;
            }
        }
    }
};