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
        Node(int value, Node* next_node = nullptr) : val(value), next(next_node) {}
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
    
    void find(int key, Node* &pred, Node* &curr) {
        retry:
        pred = head.load(std::memory_order_acquire);
        curr = pred->next.load(std::memory_order_acquire);
        
        while (true) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            
            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                Node* expected = curr;
                if (!pred->next.compare_exchange_weak(expected, unmarked_succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = unmarked_succ;
                if (curr == tail.load(std::memory_order_acquire)) {
                    return;
                }
                succ = curr->next.load(std::memory_order_acquire);
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
            curr = get_unmarked_ref(temp->next.load(std::memory_order_relaxed));
            delete temp;
        }
    }
    
    bool contains(int key) override {
        Node* curr = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        while (curr != tail.load(std::memory_order_acquire)) {
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (!is_marked_ref(succ) && curr->val == key) {
                return true;
            }
            if (curr->val > key) {
                return false;
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
            
            if (curr != tail.load(std::memory_order_acquire) && curr->val == key) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            
            Node* expected = curr;
            if (pred->next.compare_exchange_weak(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, pred, curr);
            
            if (curr == tail.load(std::memory_order_acquire) || curr->val != key) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!curr->next.compare_exchange_weak(succ, marked_succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            // Node has been marked
            
            Node* expected = curr;
            pred->next.compare_exchange_weak(expected, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            
            return true;
        }
    }
};