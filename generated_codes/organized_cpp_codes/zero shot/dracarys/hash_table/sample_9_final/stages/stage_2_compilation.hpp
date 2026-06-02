#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 128;

    struct Node {
        int val;
        std::atomic<Node*> next;
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
            Node* sentinel = new Node{INT_MIN, nullptr};
            Node* sentinel2 = new Node{INT_MAX, nullptr};
            sentinel->next = sentinel2;
            buckets[i] = sentinel;
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
        Node* current = get_unmarked_ref(buckets[index].load(std::memory_order_acquire));
        while (current != nullptr && current->val < key) {
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        if (current != nullptr && current->val == key) {
            return false;
        }
        Node* newNode = new Node{key, nullptr};
        do {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            newNode->next = next;
        } while (!current->next.compare_exchange_strong(get_unmarked_ref(next), newNode, std::memory_order_acq_rel));
        return true;
    }

    bool remove(int key) {
        std::size_t index = hash(key);
        Node* current = get_unmarked_ref(buckets[index].load(std::memory_order_acquire));
        while (current != nullptr && current->val < key) {
            current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        }
        if (current == nullptr || current->val != key) {
            return false;
        }
        Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
        do {
            Node* markedNext = get_marked_ref(next);
            if (!current->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            }
        } while (!current->next.compare_exchange_strong(get_unmarked_ref(next), get_unmarked_ref(next->next.load(std::memory_order_acquire)), std::memory_order_acq_rel));
        return true;
    }
};