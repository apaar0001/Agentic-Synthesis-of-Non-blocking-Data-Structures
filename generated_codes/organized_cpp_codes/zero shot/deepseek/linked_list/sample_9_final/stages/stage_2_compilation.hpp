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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

public:
    ConcurrentDataStructure() {
        Node* t = new Node(INT_MAX);
        Node* h = new Node(INT_MIN, t);
        head.store(h, std::memory_order_relaxed);
        tail.store(t, std::memory_order_relaxed);
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
        while (curr && curr->val < key) {
            curr = curr->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(curr);
        }
        return curr && curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (curr->val == key) {
                return false;
            }
            
            Node* newNode = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, newNode,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
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
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = unmarked;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                
                if (curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            if (curr->val != key) {
                return false;
            }
            
            Node* marked = get_marked_ref(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, marked,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            pred->next.compare_exchange_strong(curr, get_unmarked_ref(marked),
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};