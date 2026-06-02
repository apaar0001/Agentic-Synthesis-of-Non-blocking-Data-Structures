#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 1024;

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

    static size_t hash(int key) {
        return (static_cast<unsigned int>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    // Returns true if key is found, and sets pred and curr to the nodes
    // such that pred->next == curr (both unmarked) and curr->val >= key.
    bool find(int key, size_t idx, Node*& pred, Node*& curr) {
        while (true) {
            pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
            Node* succ_raw = pred->next.load(std::memory_order_acquire);
            // Skip any marked nodes that pred points to
            while (succ_raw && is_marked_ref(succ_raw)) {
                Node* succ_next = get_unmarked_ref(succ_raw->next.load(std::memory_order_acquire));
                if (pred->next.compare_exchange_strong(succ_raw, succ_next,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    succ_raw = pred->next.load(std::memory_order_acquire);
                } else {
                    succ_raw = pred->next.load(std::memory_order_acquire);
                }
            }
            curr = get_unmarked_ref(succ_raw);
            if (!curr) return false; // reached tail sentinel (should not happen)

            Node* curr_next_raw = curr->next.load(std::memory_order_acquire);
            while (curr_next_raw && is_marked_ref(curr_next_raw)) {
                Node* curr_next_next = get_unmarked_ref(curr_next_raw->next.load(std::memory_order_acquire));
                if (curr->next.compare_exchange_strong(curr_next_raw, curr_next_next,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    curr_next_raw = curr->next.load(std::memory_order_acquire);
                } else {
                    curr_next_raw = curr->next.load(std::memory_order_acquire);
                }
            }
            Node* succ = get_unmarked_ref(curr_next_raw);
            if (!curr || curr->val >= key) {
                return true;
            }
            pred = curr;
            curr = succ;
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_release);
            buckets[i].store(head, std::memory_order_release);
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
        if (!find(key, idx, pred, curr)) return false;
        return (curr->val == key);
    }

    bool add(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, idx, pred, curr)) return false; // should not happen
            if (curr->val == key) {
                return false; // duplicate
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
            // insert failed, retry
            delete node;
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, idx, pred, curr)) return false;
            if (curr->val != key) {
                return false; // not present
            }
            // logically mark curr
            Node* next = curr->next.load(std::memory_order_acquire);
            while (!is_marked_ref(next)) {
                if (curr->next.compare_exchange_weak(next, get_marked_ref(next),
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    break;
                } else {
                    next = curr->next.load(std::memory_order_acquire);
                }
            }
            // physically unlink
            Node* succ = get_unmarked_ref(next);
            if (pred->next.compare_exchange_weak(curr, succ,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                delete curr;
                return true;
            }
            // if CAS fails, retry from start
        }
    }
};