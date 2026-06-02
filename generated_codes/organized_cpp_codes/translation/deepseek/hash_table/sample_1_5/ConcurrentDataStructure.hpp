#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <memory>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 64;
    
    struct Node;
    using AtomicNodePtr = std::atomic<Node*>;
    
    struct Node {
        int key;
        AtomicNodePtr next;
        
        explicit Node(int k) : key(k), next(nullptr) {}
    };
    
    AtomicNodePtr buckets[BUCKET_COUNT];
    
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
                delete curr;
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        size_t index = hash(key);
        Node* curr = buckets[index].load(std::memory_order_acquire);
        
        while (curr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                curr = get_unmarked_ref(next);
                continue;
            }
            
            if (curr->key == key) {
                return true;
            }
            if (curr->key > key) {
                break;
            }
            curr = get_unmarked_ref(next);
        }
        return false;
    }
    
    bool add(int key) override {
        size_t index = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = buckets[index].load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) break;
                
                Node* next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    Node* unmarkedNext = get_unmarked_ref(next);
                    if (!pred) {
                        if (buckets[index].compare_exchange_strong(curr, unmarkedNext,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarkedNext;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (pred->next.compare_exchange_strong(curr, unmarkedNext,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarkedNext;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (curr->key == key) {
                    return false;
                }
                if (curr->key > key) {
                    break;
                }
                pred = curr;
                curr = get_unmarked_ref(next);
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            
            if (!pred) {
                Node* expected = buckets[index].load(std::memory_order_acquire);
                if (expected != curr) {
                    delete newNode;
                    continue;
                }
                if (buckets[index].compare_exchange_strong(expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete newNode;
            } else {
                Node* predNext = pred->next.load(std::memory_order_acquire);
                if (predNext != curr || is_marked_ref(predNext)) {
                    delete newNode;
                    continue;
                }
                if (pred->next.compare_exchange_strong(predNext, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete newNode;
            }
        }
    }
    
    bool remove(int key) override {
        size_t index = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = buckets[index].load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) return false;
                
                Node* next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    Node* unmarkedNext = get_unmarked_ref(next);
                    if (!pred) {
                        if (buckets[index].compare_exchange_strong(curr, unmarkedNext,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarkedNext;
                            continue;
                        } else {
                            break;
                        }
                    } else {
                        if (pred->next.compare_exchange_strong(curr, unmarkedNext,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = unmarkedNext;
                            continue;
                        } else {
                            break;
                        }
                    }
                }
                
                if (curr->key == key) {
                    Node* markedNext = get_marked_ref(next);
                    if (curr->next.compare_exchange_strong(next, markedNext,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // Node has been marked
                        if (!pred) {
                            buckets[index].compare_exchange_strong(curr, get_unmarked_ref(markedNext),
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        } else {
                            pred->next.compare_exchange_strong(curr, get_unmarked_ref(markedNext),
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        }
                        return true;
                    }
                    continue;
                }
                if (curr->key > key) {
                    return false;
                }
                pred = curr;
                curr = get_unmarked_ref(next);
            }
        }
    }
};