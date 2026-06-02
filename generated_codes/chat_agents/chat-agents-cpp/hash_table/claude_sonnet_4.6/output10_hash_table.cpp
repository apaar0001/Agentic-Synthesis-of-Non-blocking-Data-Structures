#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int BUCKET_COUNT = 16;

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

    size_t hash(int key) const {
        return static_cast<size_t>(key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool search(size_t b, int key,
                std::atomic<Node*>** pred_out, Node** curr_out) {
    retry:
        std::atomic<Node*>* pred = &buckets[b];
        Node* curr = pred->load(std::memory_order_acquire);
        while (true) {
            if (!get_unmarked_ref(curr)) return false;
            Node* succ = get_unmarked_ref(curr)->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                Node* unmarked = get_unmarked_ref(succ);
                if (!pred->compare_exchange_strong(curr,
                        get_unmarked_ref(curr) == curr ? unmarked : curr,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire))
                    goto retry;
                delete get_unmarked_ref(curr);
                curr = unmarked;
            } else {
                if (get_unmarked_ref(curr)->val >= key) {
                    *pred_out = pred;
                    *curr_out = get_unmarked_ref(curr);
                    return get_unmarked_ref(curr)->val == key;
                }
                pred = &get_unmarked_ref(curr)->next;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
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
        Node* cur = get_unmarked_ref(buckets[b].load(std::memory_order_acquire));
        while (cur) {
            if (!is_marked_ref(cur->next.load(std::memory_order_acquire))) {
                if (cur->val == key) return true;
                if (cur->val > key) return false;
            }
            cur = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
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
            Node* expected = curr;
            if (pred->compare_exchange_strong(expected, node,
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
            Node* marked = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire))
                continue;
            Node* expected = curr;
            pred->compare_exchange_strong(expected, succ,
                std::memory_order_acq_rel,
                std::memory_order_acquire);
            return true;
        }
    }
};
