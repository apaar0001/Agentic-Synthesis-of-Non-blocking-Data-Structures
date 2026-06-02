#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 256;
    static constexpr int SENTINEL_MIN = INT_MIN;
    static constexpr int SENTINEL_MAX = INT_MAX;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    std::size_t hash(int key) {
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

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* sentinelMin = new Node(SENTINEL_MIN);
            Node* sentinelMax = new Node(SENTINEL_MAX);
            sentinelMin->next = sentinelMax;
            buckets[i] = sentinelMin;
        }
    }

    ~ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = get_unmarked_ref(buckets[i].load(std::memory_order_relaxed));
            while (current != nullptr) {
                Node* next = get_unmarked_ref(current->next.load(std::memory_order_relaxed));
                delete current;
                current = next;
            }
        }
    }

    bool contains(int key) {
        std::size_t bucketIndex = hash(key);
        Node* head = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
        Node* current = head->next.load(std::memory_order_acquire);
        while (current != nullptr) {
            if (current->val == key) {
                return true;
            }
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) {
        std::size_t bucketIndex = hash(key);
        Node* head = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
        Node* current = head->next.load(std::memory_order_acquire);
        Node* prev = head;
        while (current != nullptr) {
            if (current->val == key) {
                return false;
            }
            if (current->val > key) {
                break;
            }
            prev = current;
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        Node* newNode = new Node(key);
        newNode->next = current;
        while (!prev->next.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
        }
        return true;
    }

    bool remove(int key) {
        std::size_t bucketIndex = hash(key);
        Node* head = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
        Node* current = head->next.load(std::memory_order_acquire);
        Node* prev = head;
        while (current != nullptr) {
            if (current->val == key) {
                Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    while (!prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
                        current = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
                    }
                    delete current;
                    return true;
                }
            }
            prev = current;
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        return false;
    }
};