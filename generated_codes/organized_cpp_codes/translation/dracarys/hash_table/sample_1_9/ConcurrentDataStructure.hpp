#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() = default;
    ~ConcurrentDataStructure() override;

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

    void deleteList(Node* node) {
        while (node) {
            Node* next = node->next.load(std::memory_order_relaxed);
            delete node;
            node = next;
        }
    }
};

ConcurrentDataStructure::~ConcurrentDataStructure() {
    for (int i = 0; i < BUCKET_COUNT; ++i) {
        deleteList(buckets[i].load(std::memory_order_relaxed));
    }
}

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
    Node* node = buckets[index].load(std::memory_order_acquire);
    if (!node) {
        Node* newNode = new Node{key, nullptr};
        Node* expected = nullptr;
        if (buckets[index].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
            return true;
        }
        delete newNode;
        return false;
    }
    if (is_marked_ref(node)) {
        node = get_unmarked_ref(node);
    }
    if (node->key == key) {
        return false;
    }
    if (node->key > key) {
        Node* newNode = new Node{key, node};
        Node* expected = node;
        if (buckets[index].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
            return true;
        }
        delete newNode;
        return false;
    }
    while (node->next.load(std::memory_order_relaxed)) {
        Node* next = node->next.load(std::memory_order_acquire);
        if (is_marked_ref(next)) {
            next = get_unmarked_ref(next);
            if (!node->next.compare_exchange_strong(next, next, std::memory_order_acq_rel)) {
                continue;
            }
        }
        if (next->key == key) {
            return false;
        }
        if (next->key > key) {
            Node* newNode = new Node{key, next};
            if (node->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                return true;
            }
            delete newNode;
            return false;
        }
        node = next;
    }
    Node* newNode = new Node{key, nullptr};
    Node* expected = nullptr;
    if (node->next.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
        return true;
    }
    delete newNode;
    return false;
}

bool ConcurrentDataStructure::remove(int key) {
    int index = hash(key);
    Node* node = buckets[index].load(std::memory_order_acquire);
    while (node) {
        if (is_marked_ref(node)) {
            node = get_unmarked_ref(node);
            continue;
        }
        if (node->key == key) {
            Node* next = node->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (!node->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                continue;
            }
            // Node has been marked
            if (!buckets[index].compare_exchange_strong(node, next, std::memory_order_acq_rel)) {
                continue;
            }
            return true;
        }
        node = node->next.load(std::memory_order_relaxed);
    }
    return false;
}