#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr std::size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* sentinelMin = new Node{INT_MIN, nullptr};
            Node* sentinelMax = new Node{INT_MAX, nullptr};
            sentinelMin->next = sentinelMax;
            buckets[i] = sentinelMin;
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = buckets[i].load(std::memory_order_relaxed);
            while (current != nullptr) {
                Node* next = get_unmarked_ref(current->next.load(std::memory_order_relaxed));
                delete current;
                current = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t index = hash(key);
        Node* current = buckets[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (current->val == key) {
                return true;
            }
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        std::size_t index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next->val < key) {
                current = next;
                next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            }
            if (next->val == key) {
                return false;
            }
            Node* newNode = new Node{key, next};
            if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        std::size_t index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next->val < key) {
                current = next;
                next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            }
            if (next->val != key) {
                return false;
            }
            Node* markedNext = get_marked_ref(next->next.load(std::memory_order_acquire));
            if (next->next.compare_exchange_strong(next->next.load(std::memory_order_acquire), markedNext, std::memory_order_acq_rel)) {
                if (current->next.compare_exchange_strong(next, next->next.load(std::memory_order_acquire), std::memory_order_acq_rel)) {
                    delete next;
                    return true;
                }
            }
        }
    }

private:
    static std::size_t hash(int key) {
        return (key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    std::atomic<Node*> buckets[BUCKET_COUNT];
};