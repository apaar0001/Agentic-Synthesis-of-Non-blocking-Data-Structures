#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr std::size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* sentinelMin = new Node(INT_MIN);
            Node* sentinelMax = new Node(INT_MAX);
            sentinelMin->next.store(sentinelMax, std::memory_order_relaxed);
            buckets[i].store(sentinelMin, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = get_unmarked_ref(buckets[i].load(std::memory_order_acquire));
            while (current != nullptr) {
                Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
                delete current;
                current = next;
            }
        }
    }

    std::size_t hash(int key) {
        return (key & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool contains(int key) {
        std::size_t index = hash(key);
        Node* current = get_unmarked_ref(buckets[index].load(std::memory_order_acquire));
        while (current != nullptr && current->val < key) {
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        return current != nullptr && current->val == key;
    }

    bool add(int key) {
        std::size_t index = hash(key);
        while (true) {
            Node* current = get_unmarked_ref(buckets[index].load(std::memory_order_acquire));
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next != nullptr && next->val <= key) {
                if (next->val == key) {
                    return false;
                }
                current = next;
                continue;
            }
            Node* newNode = new Node(key);
            newNode->next.store(next, std::memory_order_relaxed);
            if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) {
        std::size_t index = hash(key);
        while (true) {
            Node* current = get_unmarked_ref(buckets[index].load(std::memory_order_acquire));
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next == nullptr || next->val > key) {
                return false;
            }
            if (next->val == key) {
                Node* nextNext = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (current->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        delete next;
                        return true;
                    }
                }
            }
            current = next;
        }
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

private:
    std::atomic<Node*> buckets[BUCKET_COUNT];
};