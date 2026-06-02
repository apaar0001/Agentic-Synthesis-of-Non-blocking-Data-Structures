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
            sentinelMin->next.store(sentinelMax, std::memory_order_relaxed);
            buckets[i].store(sentinelMin, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* node = buckets[i].load(std::memory_order_acquire);
            while (node != nullptr) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_acquire);
        while (node != nullptr) {
            if (node->val == key) {
                return true;
            }
            node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_acquire);
        while (node != nullptr) {
            if (node->val == key) {
                return false;
            }
            if (node->val > key) {
                Node* newNode = new Node{key, node};
                if (buckets[bucketIndex].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                node = buckets[bucketIndex].load(std::memory_order_acquire);
            } else {
                node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
            }
        }
        return false;
    }

    bool remove(int key) {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_acquire);
        while (node != nullptr) {
            if (node->val == key) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                if (next == nullptr) {
                    return false;
                }
                node->next.store(get_marked_ref(next), std::memory_order_release);
                while (node->next.load(std::memory_order_acquire) == get_marked_ref(next)) {
                    if (buckets[bucketIndex].compare_exchange_strong(node, next, std::memory_order_acq_rel)) {
                        delete node;
                        return true;
                    }
                    node = buckets[bucketIndex].load(std::memory_order_acquire);
                }
                return false;
            } else if (node->val > key) {
                return false;
            } else {
                node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
            }
        }
        return false;
    }

private:
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

    std::atomic<Node*> buckets[BUCKET_COUNT];
};