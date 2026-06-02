#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstddef>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    
    struct Node {
        int key;
        std::atomic<Node*> next;
        
        explicit Node(int k) : key(k), next(nullptr) {}
    };
    
    std::atomic<Node*> buckets[BUCKET_COUNT];
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    std::size_t hash(int key) const {
        return static_cast<std::size_t>(key) % BUCKET_COUNT;
    }
    
    void search(int key, std::atomic<Node*>* head, Node*& pred, Node*& curr) {
        retry:
        pred = head->load(std::memory_order_acquire);
        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        
        while (true) {
            if (!curr) return;
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            
            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                Node* expected = curr;
                if (!pred->next.compare_exchange_strong(expected, unmarked_succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = unmarked_succ;
                if (!curr) return;
                succ = curr->next.load(std::memory_order_acquire);
            }
            
            if (curr->key >= key) return;
            pred = curr;
            curr = get_unmarked_ref(succ);
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_release);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete get_unmarked_ref(curr);
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        std::size_t index = hash(key);
        Node* curr = buckets[index].load(std::memory_order_acquire);
        
        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->key == key) {
                Node* next = unmarked_curr->next.load(std::memory_order_acquire);
                if (!is_marked_ref(next)) {
                    return true;
                }
                return false;
            }
            if (unmarked_curr->key > key) break;
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        return false;
    }
    
    bool add(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            search(key, &buckets[index], pred, curr);
            
            if (curr && curr->key == key) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_release);
            
            if (!pred) {
                Node* expected = nullptr;
                if (buckets[index].compare_exchange_strong(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
                continue;
            }
            
            Node* expected = curr;
            if (pred->next.compare_exchange_strong(expected, new_node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* pred;
            Node* curr;
            search(key, &buckets[index], pred, curr);
            
            if (!curr || curr->key != key) {
                return false;
            }
            
            Node* next = curr->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(next);
            
            if (!curr->next.compare_exchange_strong(next, marked_next,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            // Node has been marked
            
            Node* expected = curr;
            pred->next.compare_exchange_strong(expected, next,
                std::memory_order_acq_rel, std::memory_order_acquire);
            
            return true;
        }
    }
};