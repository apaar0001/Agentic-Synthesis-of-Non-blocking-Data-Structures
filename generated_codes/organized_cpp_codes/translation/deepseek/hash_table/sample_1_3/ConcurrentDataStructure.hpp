#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int k) : key(k), next(nullptr) {}
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
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
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
                return !is_marked_ref(curr);
            }
            if (unmarked_curr->key > key) {
                break;
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        
        return false;
    }
    
    bool add(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            Node* pred = nullptr;
            Node* curr = head;
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(curr)) {
                    if (!pred) {
                        buckets[index].compare_exchange_strong(head, get_unmarked_ref(succ),
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_relaxed);
                        break;
                    } else {
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ),
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_relaxed);
                        break;
                    }
                }
                
                if (unmarked_curr->key == key) {
                    return false;
                }
                if (unmarked_curr->key > key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_release);
            
            if (!pred) {
                if (buckets[index].compare_exchange_strong(head, new_node,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_relaxed)) {
                    return true;
                }
            } else {
                Node* unmarked_pred = get_unmarked_ref(pred);
                Node* expected = curr;
                if (unmarked_pred->next.compare_exchange_strong(expected, new_node,
                                                               std::memory_order_acq_rel,
                                                               std::memory_order_relaxed)) {
                    return true;
                }
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            Node* pred = nullptr;
            Node* curr = head;
            
            while (curr) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(curr)) {
                    if (!pred) {
                        buckets[index].compare_exchange_strong(head, get_unmarked_ref(succ),
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_relaxed);
                        break;
                    } else {
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ),
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_relaxed);
                        break;
                    }
                }
                
                if (unmarked_curr->key == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    
                    if (!unmarked_curr->next.compare_exchange_strong(succ, marked_succ,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_relaxed)) {
                        break;
                    }
                    
                    // Node has been marked
                    
                    if (!pred) {
                        buckets[index].compare_exchange_strong(head, get_unmarked_ref(succ),
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_relaxed);
                    } else {
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        unmarked_pred->next.compare_exchange_strong(curr, get_unmarked_ref(succ),
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_relaxed);
                    }
                    
                    return true;
                }
                if (unmarked_curr->key > key) {
                    return false;
                }
                pred = curr;
                curr = succ;
            }
        }
    }
};