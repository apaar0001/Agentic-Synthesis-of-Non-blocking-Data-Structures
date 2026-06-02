#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <memory>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 64;
    
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
    
    size_t hash(int key) const {
        return static_cast<size_t>(key) % BUCKET_COUNT;
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_relaxed);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete get_unmarked_ref(curr);
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        size_t bucket_idx = hash(key);
        Node* curr = buckets[bucket_idx].load(std::memory_order_acquire);
        
        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->key == key) {
                return !is_marked_ref(curr);
            } else if (unmarked_curr->key > key) {
                return false;
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        return false;
    }
    
    bool add(int key) override {
        size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = buckets[bucket_idx].load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) break;
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    Node* succ = unmarked_curr->next.load(std::memory_order_relaxed);
                    if (pred) {
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        Node* expected = curr;
                        if (!unmarked_pred->next.compare_exchange_strong(expected, succ,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed)) {
                            break;
                        }
                    } else {
                        Node* expected = curr;
                        if (!buckets[bucket_idx].compare_exchange_strong(expected, succ,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed)) {
                            break;
                        }
                    }
                    curr = succ;
                    continue;
                }
                
                if (unmarked_curr->key == key) {
                    return false;
                } else if (unmarked_curr->key > key) {
                    break;
                }
                
                pred = curr;
                curr = unmarked_curr->next.load(std::memory_order_acquire);
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            
            if (!pred) {
                Node* expected = buckets[bucket_idx].load(std::memory_order_relaxed);
                if (buckets[bucket_idx].compare_exchange_strong(expected, new_node,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed)) {
                    return true;
                }
            } else {
                Node* unmarked_pred = get_unmarked_ref(pred);
                if (is_marked_ref(pred)) {
                    delete new_node;
                    continue;
                }
                Node* expected = curr;
                if (unmarked_pred->next.compare_exchange_strong(expected, new_node,
                                                               std::memory_order_release,
                                                               std::memory_order_relaxed)) {
                    return true;
                }
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        size_t bucket_idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = buckets[bucket_idx].load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) return false;
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (is_marked_ref(curr)) {
                    Node* succ = unmarked_curr->next.load(std::memory_order_relaxed);
                    if (pred) {
                        Node* unmarked_pred = get_unmarked_ref(pred);
                        Node* expected = curr;
                        if (!unmarked_pred->next.compare_exchange_strong(expected, succ,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed)) {
                            break;
                        }
                    } else {
                        Node* expected = curr;
                        if (!buckets[bucket_idx].compare_exchange_strong(expected, succ,
                                                                        std::memory_order_release,
                                                                        std::memory_order_relaxed)) {
                            break;
                        }
                    }
                    curr = succ;
                    continue;
                }
                
                if (unmarked_curr->key == key) {
                    Node* succ = unmarked_curr->next.load(std::memory_order_relaxed);
                    Node* marked_succ = get_marked_ref(succ);
                    
                    if (unmarked_curr->next.compare_exchange_strong(succ, marked_succ,
                                                                   std::memory_order_release,
                                                                   std::memory_order_relaxed)) {
                        // Node has been marked
                        if (pred) {
                            Node* unmarked_pred = get_unmarked_ref(pred);
                            Node* expected = curr;
                            unmarked_pred->next.compare_exchange_strong(expected, succ,
                                                                       std::memory_order_release,
                                                                       std::memory_order_relaxed);
                        } else {
                            Node* expected = curr;
                            buckets[bucket_idx].compare_exchange_strong(expected, succ,
                                                                       std::memory_order_release,
                                                                       std::memory_order_relaxed);
                        }
                        return true;
                    }
                    continue;
                } else if (unmarked_curr->key > key) {
                    return false;
                }
                
                pred = curr;
                curr = unmarked_curr->next.load(std::memory_order_acquire);
            }
        }
    }
};