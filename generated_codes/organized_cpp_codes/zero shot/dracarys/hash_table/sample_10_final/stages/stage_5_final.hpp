#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int BUCKET_COUNT = 256;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    std::size_t hash(int key) {
        return (key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* sentinelMin = new Node(INT_MIN);
            Node* sentinelMax = new Node(INT_MAX);
            sentinelMin->next = sentinelMax;
            buckets[i] = sentinelMin;
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
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
        Node* current = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
        while (current != nullptr && current->val <= key) {
            if (current->val == key) {
                return true;
            }
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) {
        std::size_t bucketIndex = hash(key);
        while (true) {
            Node* current = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next == nullptr || next->val > key) {
                Node* newNode = new Node(key);
                newNode->next = next;
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else if (next->val == key) {
                return false;
            } else {
                current = next;
            }
        }
    }

    bool remove(int key) {
        std::size_t bucketIndex = hash(key);
        while (true) {
            Node* current = get_unmarked_ref(buckets[bucketIndex].load(std::memory_order_acquire));
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next == nullptr || next->val > key) {
                return false;
            } else if (next->val == key) {
                Node* markedNext = get_marked_ref(next->next.load(std::memory_order_acquire));
                if (next->next.compare_exchange_strong(next->next.load(std::memory_order_acquire), markedNext, std::memory_order_acq_rel)) {
                    if (current->next.compare_exchange_strong(next, next->next.load(std::memory_order_acquire), std::memory_order_acq_rel)) {
                        delete next;
                        return true;
                    }
                }
            } else {
                current = next;
            }
        }
    }

private:
    std::atomic<Node*> buckets[BUCKET_COUNT];
};