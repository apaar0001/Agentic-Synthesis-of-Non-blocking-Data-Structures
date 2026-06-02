#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int BUCKET_COUNT = 64;

    struct Node {
        int key;
        std::atomic<Node*> next;

        Node(int key) : key(key), next(nullptr) {}
    };

    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets_[i] = new Node(INT_MIN);
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* node = buckets_[i].load(std::memory_order_acquire);
            while (node != nullptr) {
                Node* next = node->next.load(std::memory_order_acquire);
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) override {
        int index = hash(key);
        Node* node = buckets_[index].load(std::memory_order_acquire);
        while (node != nullptr) {
            if (is_marked_ref(node->next.load(std::memory_order_acquire))) {
                node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
            } else if (node->key == key) {
                return true;
            }
            node = node->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        int index = hash(key);
        while (true) {
            Node* node = buckets_[index].load(std::memory_order_acquire);
            Node* prev = nullptr;
            while (node != nullptr) {
                if (is_marked_ref(node->next.load(std::memory_order_acquire))) {
                    node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                } else if (node->key == key) {
                    return false;
                } else if (node->key > key) {
                    Node* newNode = new Node(key);
                    newNode->next.store(node, std::memory_order_release);
                    if (prev != nullptr) {
                        if (prev->next.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                        node = prev->next.load(std::memory_order_acquire);
                    } else {
                        if (buckets_[index].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                        node = buckets_[index].load(std::memory_order_acquire);
                    }
                    break;
                }
                prev = node;
                node = node->next.load(std::memory_order_acquire);
            }
            Node* newNode = new Node(key);
            newNode->next.store(nullptr, std::memory_order_release);
            if (prev != nullptr) {
                if (prev->next.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                node = prev->next.load(std::memory_order_acquire);
            } else {
                if (buckets_[index].compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                node = buckets_[index].load(std::memory_order_acquire);
            }
        }
    }

    bool remove(int key) override {
        int index = hash(key);
        while (true) {
            Node* node = buckets_[index].load(std::memory_order_acquire);
            Node* prev = nullptr;
            while (node != nullptr) {
                if (is_marked_ref(node->next.load(std::memory_order_acquire))) {
                    node = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                } else if (node->key == key) {
                    if (is_marked_ref(node->next.load(std::memory_order_acquire))) {
                        return false;
                    }
                    Node* next = node->next.load(std::memory_order_acquire);
                    if (prev != nullptr) {
                        if (prev->next.compare_exchange_strong(node, get_marked_ref(next), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                        node = prev->next.load(std::memory_order_acquire);
                    } else {
                        if (buckets_[index].compare_exchange_strong(node, get_marked_ref(next), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                        node = buckets_[index].load(std::memory_order_acquire);
                    }
                    break;
                }
                prev = node;
                node = node->next.load(std::memory_order_acquire);
            }
            return false;
        }
    }

private:
    int hash(int key) {
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

    std::atomic<Node*> buckets_[BUCKET_COUNT];
};