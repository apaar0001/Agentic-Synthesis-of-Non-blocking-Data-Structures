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

    ConcurrentDataStructure() : buckets_(new std::atomic<Node*>[BUCKET_COUNT]) {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets_[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = buckets_[i].load(std::memory_order_acquire);
            while (current != nullptr) {
                Node* next = current->next.load(std::memory_order_acquire);
                delete current;
                current = next;
            }
        }
        delete[] buckets_;
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

    bool contains(int key) override {
        int index = hash(key);
        Node* current = buckets_[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current);
            } else if (current->key == key) {
                return true;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        int index = hash(key);
        Node* current = buckets_[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current);
            } else if (current->key == key) {
                return false;
            }
            Node* next = current->next.load(std::memory_order_acquire);
            if (next != nullptr && is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (next != nullptr && next->key > key) {
                Node* new_node = new Node(key);
                new_node->next.store(current->next.load(std::memory_order_acquire), std::memory_order_relaxed);
                if (current->next.compare_exchange_strong(next, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
            }
            current = next;
        }
        Node* new_node = new Node(key);
        new_node->next.store(nullptr, std::memory_order_relaxed);
        if (buckets_[index].exchange(new_node, std::memory_order_acq_rel) == nullptr) {
            return true;
        }
        delete new_node;
        return false;
    }

    bool remove(int key) override {
        int index = hash(key);
        while (true) {
            Node* current = buckets_[index].load(std::memory_order_acquire);
            while (current != nullptr) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current);
                } else if (current->key == key) {
                    Node* next = current->next.load(std::memory_order_acquire);
                    if (next != nullptr && is_marked_ref(next)) {
                        next = get_unmarked_ref(next);
                    }
                    if (next == nullptr) {
                        return false;
                    }
                    Node* marked_next = get_marked_ref(next);
                    if (current->next.compare_exchange_strong(next, marked_next, std::memory_order_acq_rel)) {
                        // Node has been marked
                        while (current->next.load(std::memory_order_acquire) != marked_next) {
                            // help
                        }
                        if (current->next.compare_exchange_strong(marked_next, next, std::memory_order_acq_rel)) {
                            return true;
                        }
                    }
                }
                current = current->next.load(std::memory_order_acquire);
            }
            return false;
        }
    }

private:
    int hash(int key) {
        return key % BUCKET_COUNT;
    }

    std::atomic<Node*>* buckets_;
};