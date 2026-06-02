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
    static constexpr size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v, Node* n) : val(v), next(n) {}
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

    static size_t hash(int key) {
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    // Helper to find predecessor and current node for a given key in a bucket
    void find(Node*& pred, Node*& curr, size_t idx, int key) {
        while (true) {
            pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
            curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                // Skip marked nodes
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    // Help unlink marked node
                    pred->next.compare_exchange_weak(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = succ;
                    continue;
                }
                if (curr->val >= key) {
                    return;
                }
                pred = curr;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN, nullptr);
            Node* tail = new Node(INT_MAX, nullptr);
            head->next.store(tail, std::memory_order_relaxed);
            tail->next.store(nullptr, std::memory_order_relaxed);
            buckets[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = get_unmarked_ref(buckets[i].load(std::memory_order_acquire));
            while (cur) {
                Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = next;
            }
        }
    }

    bool contains(int key) override {
        size_t idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(pred, curr, idx, key);
        return (curr->val == key);
    }

    bool add(int key) override {
        while (true) {
            size_t idx = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(pred, curr, idx, key);
            if (curr->val == key) {
                return false; // duplicate
            }
            Node* node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            // CAS failed, retry
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            size_t idx = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(pred, curr, idx, key);
            if (curr->val != key) {
                return false; // not present
            }
            // Logically mark curr
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (!curr->next.compare_exchange_weak(next, get_marked_ref(next),
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                if (is_marked_ref(next)) {
                    // already marked, help unlink
                    next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    continue;
                }
                // next changed, retry
                next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            // Physically unlink
            Node* succ = get_unmarked_ref(next);
            if (pred->next.compare_exchange_strong(curr, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                delete curr;
                return true;
            }
            // CAS failed, retry from start
        }
    }
};