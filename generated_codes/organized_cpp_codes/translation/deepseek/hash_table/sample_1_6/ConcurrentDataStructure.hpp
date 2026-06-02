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
                Node* next = unmarked_curr->next.load(std::memory_order_acquire);
                return !is_marked_ref(next);
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
            Node* head = buckets[bucket_idx].load(std::memory_order_acquire);
            Node* prev = nullptr;
            Node* curr = head;
            Node* next;
            
            while (true) {
                if (!curr) break;
                Node* unmarked_curr = get_unmarked_ref(curr);
                next = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(next)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!prev) {
                        if (buckets[bucket_idx].compare_exchange_strong(head, unmarked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarked_next;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        Node* unmarked_prev = get_unmarked_ref(prev);
                        if (unmarked_prev->next.compare_exchange_strong(curr, unmarked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarked_next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (unmarked_curr->key == key) {
                    return false;
                } else if (unmarked_curr->key > key) {
                    break;
                }
                prev = curr;
                curr = next;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            
            if (!prev) {
                if (buckets[bucket_idx].compare_exchange_strong(head, new_node, 
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
            } else {
                Node* unmarked_prev = get_unmarked_ref(prev);
                Node* expected = curr;
                if (unmarked_prev->next.compare_exchange_strong(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
            }
        }
    }
    
    bool remove(int key) override {
        size_t bucket_idx = hash(key);
        
        while (true) {
            Node* head = buckets[bucket_idx].load(std::memory_order_acquire);
            Node* prev = nullptr;
            Node* curr = head;
            Node* next;
            
            while (true) {
                if (!curr) return false;
                Node* unmarked_curr = get_unmarked_ref(curr);
                next = unmarked_curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(next)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!prev) {
                        if (buckets[bucket_idx].compare_exchange_strong(head, unmarked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarked_next;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        Node* unmarked_prev = get_unmarked_ref(prev);
                        if (unmarked_prev->next.compare_exchange_strong(curr, unmarked_next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarked_next;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (unmarked_curr->key == key) {
                    Node* marked_next = get_marked_ref(next);
                    if (unmarked_curr->next.compare_exchange_strong(next, marked_next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // Node has been marked
                        
                        Node* unmarked_next = get_unmarked_ref(next);
                        if (!prev) {
                            buckets[bucket_idx].compare_exchange_strong(head, unmarked_next,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        } else {
                            Node* unmarked_prev = get_unmarked_ref(prev);
                            unmarked_prev->next.compare_exchange_strong(curr, unmarked_next,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        }
                        return true;
                    }
                    continue;
                } else if (unmarked_curr->key > key) {
                    return false;
                }
                prev = curr;
                curr = next;
            }
        }
    }
};