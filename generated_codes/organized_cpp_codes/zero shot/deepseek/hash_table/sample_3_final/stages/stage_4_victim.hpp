#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

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

    std::size_t hash(int key) const {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN);
            head->next.store(tail, std::memory_order_relaxed);
            buckets[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* curr = buckets[idx].load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked_next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (curr->val == key) {
                return !is_marked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr->val > key) {
                return false;
            }
            curr = unmarked_next;
        }
        return false;
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred = buckets[idx].load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel)) {
                        goto retry_outer;
                    }
                    curr = unmarked_succ;
                    unmarked_curr = get_unmarked_ref(curr);
                    succ = unmarked_curr->next.load(std::memory_order_acquire);
                }
                if (unmarked_curr->val >= key) {
                    if (unmarked_curr->val == key) {
                        return false;
                    }
                    Node* new_node = new Node(key);
                    new_node->next.store(unmarked_curr, std::memory_order_relaxed);
                    if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        return true;
                    }
                    delete new_node;
                    goto retry_outer;
                }
                pred = unmarked_curr;
                curr = succ;
            }
        retry_outer:;
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred = buckets[idx].load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            while (true) {
                Node* unmarked_curr = get_unmarked_ref(curr);
                Node* succ = unmarked_curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel)) {
                        goto retry_outer_remove;
                    }
                    curr = unmarked_succ;
                    unmarked_curr = get_unmarked_ref(curr);
                    succ = unmarked_curr->next.load(std::memory_order_acquire);
                }
                if (unmarked_curr->val > key) {
                    return false;
                }
                if (unmarked_curr->val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (!unmarked_curr->next.compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel)) {
                        continue;
                    }
                    pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel);
                    return true;
                }
                pred = unmarked_curr;
                curr = succ;
            }
        retry_outer_remove:;
        }
    }
};