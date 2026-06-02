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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    static constexpr size_t BUCKET_COUNT = 1024;
    std::atomic<Node*> buckets[BUCKET_COUNT];

    size_t hash_key(int key) const {
        return static_cast<size_t>(key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(Node* head, int key, Node*& pred, Node*& curr, Node*& succ) {
        while (true) {
            pred = head;
            curr = pred->next.load(std::memory_order_acquire);
            
            while (true) {
                if (curr == nullptr) return false;
                
                Node* next_raw = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(next_raw)) {
                    succ = get_unmarked_ref(next_raw);
                    Node* expected = curr;
                    if (!pred->next.compare_exchange_strong(expected, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    if (curr == nullptr) return false;
                    next_raw = curr->next.load(std::memory_order_acquire);
                }

                if (is_marked_ref(next_raw)) {
                    break;
                }

                int c_val = curr->val;
                if (c_val >= key) {
                    succ = get_unmarked_ref(next_raw);
                    return c_val == key;
                }
                
                pred = curr;
                curr = get_unmarked_ref(next_raw);
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head_sentinel = new Node(INT_MIN);
            Node* tail_sentinel = new Node(INT_MAX);
            head_sentinel->next.store(tail_sentinel, std::memory_order_relaxed);
            buckets[i].store(head_sentinel, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr != nullptr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b_idx = hash_key(key);
        Node* head = buckets[b_idx].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        return find(head, key, pred, curr, succ);
    }

    bool add(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b_idx = hash_key(key);
        Node* head = buckets[b_idx].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;

        while (true) {
            if (find(head, key, pred, curr, succ)) {
                return false;
            }
            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);
            Node* expected = curr;
            if (pred->next.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b_idx = hash_key(key);
        Node* head = buckets[b_idx].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;

        while (true) {
            if (!find(head, key, pred, curr, succ)) {
                return false;
            }
            Node* expected_succ = succ;
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(expected_succ, marked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            Node* expected_curr = curr;
            pred->next.compare_exchange_strong(expected_curr, succ, std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};
