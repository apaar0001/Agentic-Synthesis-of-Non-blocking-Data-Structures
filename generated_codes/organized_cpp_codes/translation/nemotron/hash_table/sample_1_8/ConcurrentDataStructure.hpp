#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int k) : key(k), next(nullptr) {}
    };
    std::atomic<Node*> heads[BUCKET_COUNT];

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    int hash(int key) const {
        int h = key % BUCKET_COUNT;
        if (h < 0) h += BUCKET_COUNT;
        return h;
    }

    void find(int key, int bucket, Node*& pred, Node*& curr) {
        while (true) {
            pred = heads[bucket].load(std::memory_order_acquire);
            Node* succ = pred->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(succ);
            while (true) {
                Node* succ_next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(succ)) {
                    Node* desired = get_unmarked_ref(succ_next);
                    pred->next.compare_exchange_strong(succ, desired,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    succ = pred->next.load(std::memory_order_acquire);
                    curr = get_unmarked_ref(succ);
                    continue;
                }
                if (curr->key >= key) break;
                pred = curr;
                succ = succ_next;
                curr = get_unmarked_ref(succ);
            }
            Node* pred_next = pred->next.load(std::memory_order_acquire);
            if (!is_marked_ref(pred_next) && get_unmarked_ref(pred_next) == curr) {
                return;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_relaxed);
            heads[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
    }

    bool contains(int key) override {
        int b = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, b, pred, curr);
        return (curr->key == key);
    }

    bool add(int key) override {
        while (true) {
            int b = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, b, pred, curr);
            if (curr->key == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            int b = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, b, pred, curr);
            if (curr->key != key) {
                return false;
            }
            Node* curr_next = curr->next.load(std::memory_order_acquire);
            Node* marked_curr = get_marked_ref(curr);
            if (pred->next.compare_exchange_strong(curr_next, marked_curr,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};