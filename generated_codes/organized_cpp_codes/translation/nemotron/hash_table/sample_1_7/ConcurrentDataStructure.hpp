#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <utility>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int k) : key(k), next(nullptr) {}
    };
    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    static std::size_t hash(int key) {
        int idx = key % static_cast<int>(BUCKET_COUNT);
        if (idx < 0) idx += static_cast<int>(BUCKET_COUNT);
        return static_cast<std::size_t>(idx);
    }

    // Search for key, returning pred and curr such that:
    // - pred is node with key < key (or nullptr if none)
    // - curr is first node with key >= key (or nullptr if end)
    // - both pred and curr are unmarked and, if pred non-null, pred->next points to curr
    bool find(std::size_t idx, int key, Node*& pred, Node*& curr) {
        retry:
        pred = nullptr;
        curr = get_unmarked_ref(buckets[idx].load(std::memory_order_acquire));
        while (curr) {
            Node* nextRaw = curr->next.load(std::memory_order_acquire);
            // Help remove any logically deleted nodes we encounter
            while (is_marked_ref(nextRaw)) {
                Node* succ = get_unmarked_ref(nextRaw);
                std::atomic<Node*>* link = pred ? &(pred->next) : &(buckets[idx]);
                Node* expected = curr;
                if (link->compare_exchange_strong(expected, succ,
                                                  std::memory_order_acq_rel, std::memory_order_acquire)) {
                    curr = succ;
                    nextRaw = curr->next.load(std::memory_order_acquire);
                    continue;
                } else {
                    goto retry;
                }
            }
            // Now curr->next is unmarked (or null)
            if (curr->key >= key) {
                return true;
            }
            pred = curr;
            curr = get_unmarked_ref(nextRaw);
        }
        return true;
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = get_unmarked_ref(buckets[i].load(std::memory_order_acquire));
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        if (!find(idx, key, pred, curr)) {
            return false;
        }
        return (curr != nullptr && curr->key == key);
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(idx, key, pred, curr)) {
                continue;
            }
            if (curr != nullptr && curr->key == key) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->next.store(curr, std::memory_order_relaxed);
            std::atomic<Node*>* link = pred ? &(pred->next) : &(buckets[idx]);
            Node* expected = curr;
            if (link->compare_exchange_strong(expected, newNode,
                                              std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(idx, key, pred, curr)) {
                return false;
            }
            if (curr == nullptr || curr->key != key) {
                return false;
            }
            // Attempt to logically delete curr by marking its next pointer
            Node* nextRaw = curr->next.load(std::memory_order_acquire);
            Node* nextUnmarked = get_unmarked_ref(nextRaw);
            Node* markedNext = get_marked_ref(nextUnmarked);
            if (!curr->next.compare_exchange_strong(nextRaw, markedNext,
                                                  std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            return true;
        }
    }
};