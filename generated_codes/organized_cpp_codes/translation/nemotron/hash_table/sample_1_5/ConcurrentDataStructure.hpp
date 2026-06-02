#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstdint>
#include <climits>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr unsigned int BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node() : key(0), next(nullptr) {}
        explicit Node(int k) : key(k), next(nullptr) {}
    };
    Node heads[BUCKET_COUNT];
    unsigned int hash(int key) const {
        int mod = key % static_cast<int>(BUCKET_COUNT);
        if (mod < 0) mod += BUCKET_COUNT;
        return static_cast<unsigned int>(mod);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }
public:
    ConcurrentDataStructure() {
        for (unsigned int i = 0; i < BUCKET_COUNT; ++i) {
            heads[i].next.store(nullptr, std::memory_order_relaxed);
        }
    }
    ~ConcurrentDataStructure() override = default;

    bool contains(int key) override {
        unsigned int idx = hash(key);
        Node* pred = &heads[idx];
        while (true) {
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
                    curr = succ;
                } else {
                    pred = &heads[idx];
                    break;
                }
            }
            if (!curr) return false;
            if (curr->key >= key) break;
            pred = curr;
        }
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        return (curr && curr->key == key);
    }

    bool add(int key) override {
        retry:
        unsigned int idx = hash(key);
        Node* pred = &heads[idx];
        while (true) {
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (!pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = succ;
            }
            if (!curr || curr->key >= key) break;
            pred = curr;
        }
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        if (curr && curr->key == key) return false;
        Node* node = new Node(key);
        node->next.store(curr, std::memory_order_relaxed);
        if (pred->next.compare_exchange_strong(curr, node,
                                              std::memory_order_acq_rel, std::memory_order_acquire)) {
            return true;
        }
        goto retry;
    }

    bool remove(int key) override {
        retry:
        unsigned int idx = hash(key);
        Node* pred = &heads[idx];
        while (true) {
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (!pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = succ;
            }
            if (!curr || curr->key >= key) return false;
            if (curr->key > key) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked = get_marked_ref(curr);
            if (!pred->next.compare_exchange_strong(curr, marked,
                                                  std::memory_order_acq_rel, std::memory_order_acquire)) {
                goto retry;
            }
            // Node has been marked
            pred->next.compare_exchange_strong(marked, succ,
                                              std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};