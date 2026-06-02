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
                Node* temp = get_unmarked_ref(curr);
                curr = temp->next.load(std::memory_order_acquire);
                delete get_unmarked_ref(temp);
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
                return false;
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        
        return false;
    }
    
    bool add(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            
            if (!head) {
                Node* new_node = new Node(key);
                if (buckets[index].compare_exchange_strong(head, new_node,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
                continue;
            }
            
            Node* unmarked_head = get_unmarked_ref(head);
            if (unmarked_head->key == key) {
                if (!is_marked_ref(head)) {
                    return false;
                }
                Node* succ = unmarked_head->next.load(std::memory_order_acquire);
                Node* new_node = new Node(key);
                new_node->next.store(succ, std::memory_order_release);
                if (buckets[index].compare_exchange_strong(head, new_node,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
                continue;
            }
            
            if (unmarked_head->key > key) {
                Node* new_node = new Node(key);
                new_node->next.store(head, std::memory_order_release);
                if (buckets[index].compare_exchange_strong(head, new_node,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
                continue;
            }
            
            Node* pred = head;
            Node* curr = unmarked_head->next.load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) {
                    Node* new_node = new Node(key);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (unmarked_pred->next.compare_exchange_strong(curr, new_node,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                        return true;
                    }
                    delete new_node;
                    break;
                }
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->key == key) {
                    if (!is_marked_ref(curr)) {
                        return false;
                    }
                    Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                    Node* new_node = new Node(key);
                    new_node->next.store(succ, std::memory_order_release);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (unmarked_pred->next.compare_exchange_strong(curr, new_node,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                        return true;
                    }
                    delete new_node;
                    break;
                }
                
                if (unmarked_curr->key > key) {
                    Node* new_node = new Node(key);
                    new_node->next.store(curr, std::memory_order_release);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (unmarked_pred->next.compare_exchange_strong(curr, new_node,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                        return true;
                    }
                    delete new_node;
                    break;
                }
                
                if (is_marked_ref(curr)) {
                    Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (!unmarked_pred->next.compare_exchange_strong(curr, succ,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                pred = curr;
                curr = unmarked_curr->next.load(std::memory_order_acquire);
            }
        }
    }
    
    bool remove(int key) override {
        std::size_t index = hash(key);
        
        while (true) {
            Node* head = buckets[index].load(std::memory_order_acquire);
            
            if (!head) {
                return false;
            }
            
            Node* unmarked_head = get_unmarked_ref(head);
            if (unmarked_head->key == key) {
                if (is_marked_ref(head)) {
                    return false;
                }
                Node* succ = unmarked_head->next.load(std::memory_order_acquire);
                Node* marked_head = get_marked_ref(head);
                if (buckets[index].compare_exchange_strong(head, marked_head,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                    // Node has been marked
                    return true;
                }
                continue;
            }
            
            if (unmarked_head->key > key) {
                return false;
            }
            
            Node* pred = head;
            Node* curr = unmarked_head->next.load(std::memory_order_acquire);
            
            while (true) {
                if (!curr) {
                    return false;
                }
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->key == key) {
                    if (is_marked_ref(curr)) {
                        return false;
                    }
                    Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                    Node* marked_curr = get_marked_ref(curr);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (unmarked_pred->next.compare_exchange_strong(curr, marked_curr,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_acquire)) {
                        // Node has been marked
                        return true;
                    }
                    break;
                }
                
                if (unmarked_curr->key > key) {
                    return false;
                }
                
                if (is_marked_ref(curr)) {
                    Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                    Node* unmarked_pred = get_unmarked_ref(pred);
                    if (!unmarked_pred->next.compare_exchange_strong(curr, succ,
                                                                    std::memory_order_acq_rel,
                                                                    std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                
                pred = curr;
                curr = unmarked_curr->next.load(std::memory_order_acquire);
            }
        }
    }
};