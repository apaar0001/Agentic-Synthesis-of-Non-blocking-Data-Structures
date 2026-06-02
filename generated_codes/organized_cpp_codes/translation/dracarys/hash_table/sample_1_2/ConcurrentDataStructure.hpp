#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : buckets() {
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

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }
};

bool ConcurrentDataStructure::contains(int key) {
    int index = hash(key);
    Node* node = buckets[index].load(std::memory_order_acquire);
    while (node) {
        if (is_marked_ref(node)) {
            node = get_unmarked_ref(node);
            continue;
        }
        if (node->key == key) {
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
        Node* prev = nullptr;
        Node* curr = node;
        while (curr) {
            if (is_marked_ref(curr)) {
                curr = get_unmarked_ref(curr);
                continue;
            }
            if (curr->key == key) {
                return false;
            }
            if (curr->key > key) {
                Node* newNode = new Node{key, curr};
                if (buckets[index].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                break;
            }
            prev = curr;
            curr = curr->next.load(std::memory_order_acquire);
        }
        if (prev) {
            Node* newNode = new Node{key, nullptr};
            if (prev->next.compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
        } else {
            Node* newNode = new Node{key, nullptr};
            if (buckets[index].compare_exchange_strong(nullptr, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
        }
    }
    return false;
}

bool ConcurrentDataStructure::remove(int key) {
    int index = hash(key);
    while (true) {
        Node* node = buckets[index].load(std::memory_order_acquire);
        Node* prev = nullptr;
        Node* curr = node;
        while (curr) {
            if (is_marked_ref(curr)) {
                curr = get_unmarked_ref(curr);
                continue;
            }
            if (curr->key == key) {
                Node* next = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    return false;
                }
                if (curr->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (prev) {
                        prev->next.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    } else {
                        buckets[index].compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    }
                    return true;
                }
                continue;
            }
            if (curr->key > key) {
                return false;
            }
            prev = curr;
            curr = curr->next.load(std::memory_order_acquire);
        }
    }
    return false;
}