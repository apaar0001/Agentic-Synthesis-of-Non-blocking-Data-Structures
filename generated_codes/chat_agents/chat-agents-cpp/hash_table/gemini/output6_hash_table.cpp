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

        Node(int key) : val(key), next(nullptr) {}
    };

    static constexpr size_t BUCKET_COUNT = 1024;

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    size_t hash_key(int key) const {
        return static_cast<size_t>(key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(Node* head, int key, Node*& pred, Node*& curr, Node*& succ) {
        while (true) {
            pred = head;
            curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                if (curr == nullptr) return false;
                
                Node* curr_unmarked = get_unmarked_ref(curr);
                Node* next_raw = curr_unmarked->next.load(std::memory_order_acquire);
                
                while (is_marked_ref(next_raw)) {
                    succ = get_unmarked_ref(next_raw);
                    if (!pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    if (curr == nullptr) return false;
                    curr_unmarked = get_unmarked_ref(curr);
                    next_raw = curr_unmarked->next.load(std::memory_order_acquire);
                }
                
                if (is_marked_ref(next_raw)) {
                    break;
                }
                
                if (curr_unmarked->val >= key) {
                    return curr_unmarked->val == key;
                }
                
                pred = curr_unmarked;
                curr = next_raw;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_relaxed);
            buckets[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr != nullptr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete get_unmarked_ref(curr);
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b = hash_key(key);
        Node* head = buckets[b].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        return find(head, key, pred, curr, succ);
    }

    bool add(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b = hash_key(key);
        Node* head = buckets[b].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
        while (true) {
            if (find(head, key, pred, curr, succ)) {
                return false;
            }
            
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b = hash_key(key);
        Node* head = buckets[b].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        
        while (true) {
            if (!find(head, key, pred, curr, succ)) {
                return false;
            }
            
            Node* curr_unmarked = get_unmarked_ref(curr);
            Node* next_raw = curr_unmarked->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_raw)) {
                continue;
            }
            
            Node* marked_succ = get_marked_ref(next_raw);
            if (!curr_unmarked->next.compare_exchange_strong(next_raw, marked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            
            if (pred->next.compare_exchange_strong(curr, next_raw, std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Physical deletion successful
            }
            return true;
        }
    }
};
