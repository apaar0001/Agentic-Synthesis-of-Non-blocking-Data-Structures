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

    size_t hash_fn(int key) const {
        return static_cast<size_t>(key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(Node* head, int key, Node** pred, Node** curr, Node** succ) {
        while (true) {
            *pred = head;
            *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));

            while (true) {
                if (*curr == nullptr) return false;

                Node* succ_raw = (*curr)->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ_raw)) {
                    Node* unmarked_succ = get_unmarked_ref(succ_raw);
                    Node* expected = *curr;
                    if (!(*pred)->next.compare_exchange_weak(expected, unmarked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    *curr = unmarked_succ;
                    if (*curr == nullptr) return false;
                    succ_raw = (*curr)->next.load(std::memory_order_acquire);
                }

                if (!is_marked_ref(succ_raw)) {
                    *succ = get_unmarked_ref(succ_raw);
                    if ((*curr)->val >= key) {
                        return (*curr)->val == key;
                    }
                    *pred = *curr;
                    *curr = *succ;
                } else {
                    break;
                }
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* min_sentinel = new Node(INT_MIN);
            Node* max_sentinel = new Node(INT_MAX);
            min_sentinel->next.store(max_sentinel, std::memory_order_relaxed);
            buckets[i].store(min_sentinel, std::memory_order_relaxed);
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
        size_t idx = hash_fn(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);
        Node* pred;
        Node* curr;
        Node* succ;
        return find(head, key, &pred, &curr, &succ);
    }

    bool add(int key) override {
        size_t idx = hash_fn(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);
        Node* pred;
        Node* curr;
        Node* succ;

        while (true) {
            if (find(head, key, &pred, &curr, &succ)) {
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
        size_t idx = hash_fn(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);
        Node* pred;
        Node* curr;
        Node* succ;

        while (true) {
            if (!find(head, key, &pred, &curr, &succ)) {
                return false;
            }
            Node* expected_succ = succ;
            if (!curr->next.compare_exchange_strong(expected_succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            Node* expected_curr = curr;
            pred->next.compare_exchange_strong(expected_curr, succ, std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};
