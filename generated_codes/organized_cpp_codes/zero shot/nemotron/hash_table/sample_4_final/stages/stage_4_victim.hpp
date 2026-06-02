#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v, Node* n) : val(v), next(n) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static std::size_t hash(int key) {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
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
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* minNode = new Node(INT_MIN, nullptr);
            Node* maxNode = new Node(INT_MAX, nullptr);
            minNode->next.store(maxNode, std::memory_order_relaxed);
            buckets[i].store(minNode, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = buckets[i].load(std::memory_order_acquire);
            while (cur) {
                Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = next;
            }
        }
    }

    bool contains(int key) override {
        size_t idx = hash(key);
        Node* prev = buckets[idx].load(std::memory_order_acquire);
        Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
        while (curr) {
            while (curr && is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                prev->next.compare_exchange_strong(curr, next,
                    std::memory_order_acq_rel, std::memory_order_acquire);
                curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            }
            if (!curr || curr->val >= key) break;
            if (curr->val == key) return true;
            prev = curr;
            curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* prev = buckets[idx].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            while (curr) {
                while (curr && is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    prev->next.compare_exchange_strong(curr, next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->val >= key) break;
                prev = curr;
                curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            }
            if (curr && curr->val == key) return false;
            Node* node = new Node(key, nullptr);
            node->next.store(curr, std::memory_order_relaxed);
            if (prev->next.compare_exchange_strong(curr, node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            // else retry
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* prev = buckets[idx].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            while (curr) {
                while (curr && is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    prev->next.compare_exchange_strong(curr, next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->val >= key) break;
                prev = curr;
                curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            }
            if (!curr || curr->val != key) return false;
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            // mark node
            if (!curr->next.compare_exchange_strong(next, get_marked_ref(next),
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // unlink
            prev->next.compare_exchange_strong(curr, next,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};