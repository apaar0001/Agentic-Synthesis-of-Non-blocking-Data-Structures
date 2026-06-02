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

        Node(int val) : val(val), next(nullptr) {}
    };

    ConcurrentDataStructure() {
        for (auto& bucket : buckets) {
            Node* sentinelMin = new Node(INT_MIN);
            Node* sentinelMax = new Node(INT_MAX);
            sentinelMin->next = sentinelMax;
            bucket = sentinelMin;
        }
    }

    ~ConcurrentDataStructure() override {
        for (auto& bucket : buckets) {
            Node* node = bucket.load(std::memory_order_relaxed);
            while (node != nullptr) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_relaxed));
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_relaxed);
        while (node != nullptr) {
            if (node->val == key) {
                return true;
            }
            node = get_unmarked_ref(node->next.load(std::memory_order_relaxed));
        }
        return false;
    }

    bool add(int key) override {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_relaxed);
        Node* prev = nullptr;
        Node* curr = node;
        while (curr != nullptr) {
            if (curr->val == key) {
                return false;
            }
            if (curr->val > key) {
                break;
            }
            prev = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
        }
        Node* newNode = new Node(key);
        newNode->next = curr;
        while (!prev->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
            curr = get_unmarked_ref(prev->next.load(std::memory_order_relaxed));
            if (curr->val == key) {
                delete newNode;
                return false;
            }
        }
        return true;
    }

    bool remove(int key) override {
        std::size_t bucketIndex = hash(key);
        Node* node = buckets[bucketIndex].load(std::memory_order_relaxed);
        Node* prev = nullptr;
        Node* curr = node;
        while (curr != nullptr) {
            if (curr->val == key) {
                break;
            }
            prev = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
        }
        if (curr == nullptr || curr->val != key) {
            return false;
        }
        Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
        while (!curr->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
            next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
        }
        while (!prev->next.compare_exchange_strong(curr, next, std::memory_order_acq_rel)) {
            curr = get_unmarked_ref(prev->next.load(std::memory_order_relaxed));
        }
        delete curr;
        return true;
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