#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr std::size_t BUCKET_COUNT = 256;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

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

    ConcurrentDataStructure() {
        for (auto& bucket : buckets) {
            Node* sentinelMin = new Node(INT_MIN);
            Node* sentinelMax = new Node(INT_MAX);
            sentinelMin->next = sentinelMax;
            bucket = sentinelMin;
        }
    }

    ~ConcurrentDataStructure() {
        for (auto& bucket : buckets) {
            Node* node = bucket;
            while (node != nullptr) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) override {
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

    bool add(int key) override {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_acquire);
        while (node != nullptr) {
            if (node->val == key) {
                return false;
            }
            if (node->val > key) {
                Node* newNode = new Node(key);
                Node* next = node;
                newNode->next = next;
                while (!buckets[bucketIndex].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    node = buckets[bucketIndex].load(std::memory_order_acquire);
                    if (node->val == key) {
                        delete newNode;
                        return false;
                    }
                    if (node->val > key) {
                        next = node;
                        newNode->next = next;
                    } else {
                        delete newNode;
                        return false;
                    }
                }
                return true;
            }
            node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool remove(int key) override {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_acquire);
        Node* prev = nullptr;
        while (node != nullptr) {
            if (node->val == key) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                Node* markedNext = get_marked_ref(next);
                if (node->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                    while (true) {
                        Node* newNext = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                        if (next->next.compare_exchange_strong(newNext, markedNext, std::memory_order_acq_rel)) {
                            break;
                        }
                        next = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                    }
                    if (prev != nullptr) {
                        prev->next.compare_exchange_strong(node, next, std::memory_order_acq_rel);
                    } else {
                        buckets[bucketIndex].compare_exchange_strong(node, next, std::memory_order_acq_rel);
                    }
                    return true;
                }
            }
            prev = node;
            node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
        }
        return false;
    }

private:
    std::atomic<Node*> buckets[BUCKET_COUNT];
};