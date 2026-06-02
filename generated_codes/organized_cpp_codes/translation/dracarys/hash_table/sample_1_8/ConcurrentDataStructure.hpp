#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    bool contains(int key) override;
    bool add(int key) override;
    bool remove(int key) override;

private:
    static constexpr int BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    int hash(int key) const {
        return key % BUCKET_COUNT;
    }

    static bool is_marked_ref(uintptr_t ref) {
        return ref & 1;
    }

    static uintptr_t get_unmarked_ref(uintptr_t ref) {
        return ref & ~1;
    }

    static uintptr_t get_marked_ref(uintptr_t ref) {
        return ref | 1;
    }
};

bool ConcurrentDataStructure::contains(int key) {
    int index = hash(key);
    Node* node = buckets[index].load(std::memory_order_acquire);
    while (node) {
        if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
            node = reinterpret_cast<Node*>(get_unmarked_ref(reinterpret_cast<uintptr_t>(node)));
        } else if (node->key == key) {
            return true;
        }
        node = node->next.load(std::memory_order_acquire);
    }
    return false;
}

bool ConcurrentDataStructure::add(int key) {
    int index = hash(key);
    while (true) {
        Node* node = buckets[index].load(std::memory_order_acquire);
        while (node) {
            if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
                node = reinterpret_cast<Node*>(get_unmarked_ref(reinterpret_cast<uintptr_t>(node)));
            } else if (node->key == key) {
                return false;
            } else if (node->key > key) {
                Node* newNode = new Node{key, node};
                if (buckets[index].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                node = buckets[index].load(std::memory_order_acquire);
            } else {
                Node* next = node->next.load(std::memory_order_acquire);
                if (is_marked_ref(reinterpret_cast<uintptr_t>(next))) {
                    next = reinterpret_cast<Node*>(get_unmarked_ref(reinterpret_cast<uintptr_t>(next)));
                }
                if (next && next->key < key) {
                    node = next;
                } else {
                    Node* newNode = new Node{key, next};
                    if (node->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                    delete newNode;
                    node = buckets[index].load(std::memory_order_acquire);
                }
            }
        }

        Node* newNode = new Node{key, nullptr};
        Node* expected = nullptr;
        if (buckets[index].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
            return true;
        }
        delete newNode;
    }
}

bool ConcurrentDataStructure::remove(int key) {
    int index = hash(key);
    while (true) {
        Node* node = buckets[index].load(std::memory_order_acquire);
        while (node) {
            if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
                node = reinterpret_cast<Node*>(get_unmarked_ref(reinterpret_cast<uintptr_t>(node)));
            } else if (node->key == key) {
                Node* next = node->next.load(std::memory_order_acquire);
                if (is_marked_ref(reinterpret_cast<uintptr_t>(next))) {
                    return false;
                }
                Node* markedNext = reinterpret_cast<Node*>(get_marked_ref(reinterpret_cast<uintptr_t>(next)));
                if (node->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (buckets[index].load(std::memory_order_acquire) == node) {
                        buckets[index].compare_exchange_strong(node, next, std::memory_order_acq_rel);
                    }
                    return true;
                }
                return false;
            }
            node = node->next.load(std::memory_order_acquire);
        }
        return false;
    }
}