#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 1024;
    
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };
    
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }
    
    std::atomic<Node*> buckets[BUCKET_COUNT];
    
    std::size_t hash(int key) const {
        return (key & 0x7FFFFFFF) % BUCKET_COUNT;
    }
    
    Node* create_sentinel_list() {
        Node* tail = new Node(INT_MAX);
        tail->next.store(nullptr, std::memory_order_relaxed);
        Node* head = new Node(INT_MIN);
        head->next.store(tail, std::memory_order_relaxed);
        return head;
    }
    
    bool search_bucket(std::size_t idx, int key, Node*& pred, Node*& curr) {
        Node* pred_next;
        
        retry:
        pred = buckets[idx].load(std::memory_order_acquire);
        curr = pred->next.load(std::memory_order_acquire);
        
        while (true) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (is_marked_ref(curr)) {
                unmarked_curr = get_unmarked_ref(curr);
            }
            
            while (is_marked_ref(unmarked_curr->next.load(std::memory_order_acquire))) {
                Node* unmarked_next = get_unmarked_ref(unmarked_curr->next.load(std::memory_order_acquire));
                if (pred->next.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel)) {
                    curr = unmarked_next;
                    unmarked_curr = get_unmarked_ref(curr);
                } else {
                    goto retry;
                }
            }
            
            if (unmarked_curr->val >= key) {
                pred_next = pred->next.load(std::memory_order_acquire);
                if (is_marked_ref(pred_next) || pred != buckets[idx].load(std::memory_order_acquire)) {
                    goto retry;
                }
                curr = unmarked_curr;
                return curr->val == key;
            }
            
            pred = unmarked_curr;
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
    }
    
public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(create_sentinel_list(), std::memory_order_relaxed);
        }
    }
    
    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = buckets[i].load(std::memory_order_relaxed);
            while (head) {
                Node* unmarked_next = get_unmarked_ref(head->next.load(std::memory_order_relaxed));
                Node* to_delete = head;
                head = unmarked_next;
                delete to_delete;
            }
        }
    }
    
    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        bool found = search_bucket(idx, key, pred, curr);
        return found && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }
    
    bool add(int key) override {
        std::size_t idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            
            if (search_bucket(idx, key, pred, curr)) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            if (is_marked_ref(pred_next)) {
                delete new_node;
                continue;
            }
            
            if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel)) {
                return true;
            }
            
            delete new_node;
        }
    }
    
    bool remove(int key) override {
        std::size_t idx = hash(key);
        
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            
            if (!search_bucket(idx, key, pred, curr)) {
                return false;
            }
            
            Node* succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            
            if (!curr->next.compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                continue;
            }
            
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            if (is_marked_ref(pred_next) || pred != buckets[idx].load(std::memory_order_acquire)) {
                continue;
            }
            
            if (pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel)) {
                delete get_unmarked_ref(curr);
            }
            
            return true;
        }
    }
};