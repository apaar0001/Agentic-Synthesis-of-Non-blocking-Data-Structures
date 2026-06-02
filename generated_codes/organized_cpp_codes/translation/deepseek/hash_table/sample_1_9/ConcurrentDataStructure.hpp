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
        return reinterpret_cast<uintptr_t>(ptr) & 1;
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
    
    bool listContains(Node* head, int key) const {
        Node* current = head;
        while (current) {
            Node* curr_next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(curr_next)) {
                current = get_unmarked_ref(curr_next);
                continue;
            }
            if (current->key == key) {
                return !is_marked_ref(current->next.load(std::memory_order_acquire));
            }
            if (current->key > key) {
                break;
            }
            current = get_unmarked_ref(curr_next);
        }
        return false;
    }
    
    bool listAdd(std::atomic<Node*>& head, int key) {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred;
            
            while (true) {
                if (!curr) break;
                Node* curr_next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(curr_next)) {
                    Node* unmarked_next = get_unmarked_ref(curr_next);
                    if (pred->next.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel)) {
                        curr = unmarked_next;
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr->key == key) {
                    return false;
                }
                if (curr->key > key) {
                    break;
                }
                pred = curr;
                curr = get_unmarked_ref(curr_next);
            }
            
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_release);
            
            if (pred == head.load(std::memory_order_acquire)) {
                if (pred->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            }
            delete newNode;
        }
    }
    
    bool listRemove(std::atomic<Node*>& head, int key) {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred;
            
            while (true) {
                if (!curr) return false;
                Node* curr_next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(curr_next)) {
                    Node* unmarked_next = get_unmarked_ref(curr_next);
                    if (pred->next.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel)) {
                        curr = unmarked_next;
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr->key == key) {
                    break;
                }
                if (curr->key > key) {
                    return false;
                }
                pred = curr;
                curr = get_unmarked_ref(curr_next);
            }
            
            if (!curr) return false;
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                continue;
            }
            // Node has been marked
            
            Node* unmarked_succ = get_unmarked_ref(succ);
            pred->next.compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel);
            
            return true;
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_release);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }
    
    bool contains(int key) override {
        size_t index = hash(key);
        Node* head = buckets[index].load(std::memory_order_acquire);
        return listContains(head, key);
    }
    
    bool add(int key) override {
        size_t index = hash(key);
        return listAdd(buckets[index], key);
    }
    
    bool remove(int key) override {
        size_t index = hash(key);
        return listRemove(buckets[index], key);
    }
};