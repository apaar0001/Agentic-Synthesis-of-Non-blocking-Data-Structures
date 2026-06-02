#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 256;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL);
    }

    static uint32_t fnv_mix(uint32_t k) {
        k ^= 2166136261u;
        k *= 16777619u;
        k ^= k >> 16;
        return k;
    }

    size_t hash(int key) const {
        return static_cast<size_t>(fnv_mix(static_cast<uint32_t>(key))) % BUCKET_COUNT;
    }

    bool search(size_t b, int key,
                std::atomic<Node*>** pred_out, Node** curr_out) {
    retry:
        std::atomic<Node*>* pred = &buckets[b];
        Node* curr = pred->load(std::memory_order_acquire);
        while (true) {
            Node* uc = get_unmarked_ref(curr);
            if (!uc) return false;
            Node* succ_raw = uc->next.load(std::memory_order_acquire);
            bool marked = is_marked_ref(succ_raw);
            Node* succ = get_unmarked_ref(succ_raw);
            if (marked) {
                Node* exp = uc;
                if (!pred->compare_exchange_strong(exp, succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                    goto retry;
                delete uc;
                curr = succ;
            } else {
                if (uc->val >= key) {
                    *pred_out = pred;
                    *curr_out = uc;
                    return uc->val == key;
                }
                pred = &uc->next;
                curr = succ_raw;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = get_unmarked_ref(buckets[i].load(std::memory_order_relaxed));
            while (cur) {
                Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
                delete cur;
                cur = nxt;
            }
        }
    }

    bool contains(int key) override {
        size_t b = hash(key);
        Node* cur = get_unmarked_ref(buckets[b].load(std::memory_order_relaxed));
        while (cur) {
            Node* nxt_raw = cur->next.load(std::memory_order_relaxed);
            if (!is_marked_ref(nxt_raw) && cur->val == key) return true;
            if (cur->val > key) return false;
            cur = get_unmarked_ref(nxt_raw);
        }
        return false;
    }

    bool add(int key) override {
        size_t b = hash(key);
        while (true) {
            std::atomic<Node*>* pred;
            Node* curr;
            if (search(b, key, &pred, &curr)) return false;
            Node* node = new Node(key, curr);
            Node* exp = curr;
            if (pred->compare_exchange_strong(exp, node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
                return true;
            delete node;
        }
    }

    bool remove(int key) override {
        size_t b = hash(key);
        while (true) {
            std::atomic<Node*>* pred;
            Node* curr;
            if (!search(b, key, &pred, &curr)) return false;
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) continue;
            Node* marked = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
                continue;
            Node* exp = curr;
            pred->compare_exchange_strong(exp, succ,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
            return true;
        }
    }
};
