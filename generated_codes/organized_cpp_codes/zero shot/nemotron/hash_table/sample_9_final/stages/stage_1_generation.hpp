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
        Node(int v, Node* n) : val(v), next(n) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static size_t hash(int key) {
        return (static_cast<size_t>(static_cast<unsigned int>(key)) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN, nullptr);
            Node* tail = new Node(INT_MAX, nullptr);
            head->next.store(tail, std::memory_order_relaxed);
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
        Node* pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            // skip marked nodes
            Node* succ = curr->next.load(std::memory_order_acquire);
            while (is_marked_ref(succ)) {
                Node* succNext = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                bool removed = pred->next.compare_exchange_strong(curr, succNext,
                                                                  std::memory_order_acq_rel,
                                                                  std::memory_order_acquire);
                if (!removed) {
                    pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
                    curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                    break;
                }
                curr = succNext;
                succ = curr->next.load(std::memory_order_acquire);
            }
            if (curr->val >= key) break;
            pred = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                // skip marked nodes
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* succNext = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                    bool removed = pred->next.compare_exchange_strong(curr, succNext,
                                                                      std::memory_order_acq_rel,
                                                                      std::memory_order_acquire);
                    if (!removed) {
                        pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
                        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                        break;
                    }
                    curr = succNext;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                if (curr->val >= key) break;
                pred = curr;
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return false;
            }
            Node* node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                // skip marked nodes
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* succNext = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                    bool removed = pred->next.compare_exchange_strong(curr, succNext,
                                                                      std::memory_order_acq_rel,
                                                                      std::memory_order_acquire);
                    if (!removed) {
                        pred = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
                        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                        break;
                    }
                    curr = succNext;
                    succ = curr->next.load(std::memory_order_acquire);
                }
                if (curr->val >= key) break;
                pred = curr;
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr->val != key || is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return false;
            }
            Node* succ = curr->next.load(std::memory_order_acquire);
            while (!is_marked_ref(succ)) {
                Node* markedSucc = get_marked_ref(succ);
                if (curr->next.compare_exchange_weak(succ, markedSucc,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    break;
                } else {
                    succ = curr->next.load(std::memory_order_acquire);
                }
            }
            return true;
        }
    }
};