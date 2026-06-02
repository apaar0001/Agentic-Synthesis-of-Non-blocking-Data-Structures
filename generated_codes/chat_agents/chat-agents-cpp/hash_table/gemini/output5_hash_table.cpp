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

    size_t hash(int key) const {
        return static_cast<size_t>(key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(size_t bucket_idx, int key, Node** pred, Node** curr, Node** succ) {
        while (true) {
            *pred = buckets[bucket_idx].load(std::memory_order_acquire);
            *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));

            while (true) {
                Node* next_raw = (*curr)->next.load(std::memory_order_acquire);
                while (is_marked_ref(next_raw)) {
                    *succ = get_unmarked_ref(next_raw);
                    Node* expected = *curr;
                    if (!(*pred)->next.compare_exchange_strong(expected, *succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    *curr = *succ;
                    next_raw = (*curr)->next.load(std::memory_order_acquire);
                }

                if ((*curr)->val >= key) {
                    *succ = get_unmarked_ref(next_raw);
                    return (*curr)->val == key;
                }

                *pred = *curr;
                *curr = get_unmarked_ref(next_raw);
            }
        retry:;
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
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b = hash(key);
        Node* curr = get_unmarked_ref(buckets[b].load(std::memory_order_acquire)->next.load(std::memory_order_acquire));
        while (curr != nullptr) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            if (!is_marked_ref(next_raw)) {
                if (curr->val >= key) {
                    return curr->val == key;
                }
            }
            curr = get_unmarked_ref(next_raw);
        }
        return false;
    }

    bool add(int key) override {
        if (key <= INT_MIN || key >= INT_MAX) return false;
        size_t b = hash(key);
        Node* pred;
        Node* curr;
        Node* succ;

        while (true) {
            if (find(b, key, &pred, &curr, &succ)) {
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
        size_t b = hash(key);
        Node* pred;
        Node* curr;
        Node* succ;

        while (true) {
            if (!find(b, key, &pred, &curr, &succ)) {
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
