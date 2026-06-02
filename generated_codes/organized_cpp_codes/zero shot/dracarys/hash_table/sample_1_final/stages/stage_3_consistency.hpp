#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr std::size_t BUCKET_COUNT = 128;

    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    ConcurrentDataStructure() {
        for (auto& bucket : buckets) {
            Node* sentinel = new Node{INT_MIN, nullptr};
            Node* sentinel2 = new Node{INT_MAX, nullptr};
            sentinel->next.store(sentinel2, std::memory_order_relaxed);
            bucket.store(sentinel, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (auto& bucket : buckets) {
            Node* current = bucket.load(std::memory_order_acquire);
            while (current != nullptr) {
                Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
                delete current;
                current = next;
            }
        }
    }

    bool contains(int key) {
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

    bool add(int key) {
        std::size_t index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next->val >= key) {
                if (next->val == key) {
                    return false;
                }
                Node* newNode = new Node{key, next};
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                current = next;
            }
        }
    }

    bool remove(int key) {
        std::size_t index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next->val >= key) {
                if (next->val == key) {
                    Node* next_next = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                    next->next.store(get_marked_ref(next_next), std::memory_order_release);
                    if (current->next.compare_exchange_strong(next, next_next, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    return false;
                }
            } else {
                current = next;
            }
        }
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