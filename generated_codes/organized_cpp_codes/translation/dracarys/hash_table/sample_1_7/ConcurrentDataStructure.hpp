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

    ConcurrentDataStructure() : buckets_(new Node*[BUCKET_COUNT]()) {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets_[i] = nullptr;
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = buckets_[i];
            while (current != nullptr) {
                Node* next = current->next.load(std::memory_order_acquire);
                delete current;
                current = next;
            }
        }
        delete[] buckets_;
    }

    bool contains(int key) override {
        int index = hash(key);
        Node* current = buckets_[index];
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
        Node* current = buckets_[index];
        while (current != nullptr) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current);
            } else if (current->key == key) {
                return false;
            }
            if (current->key > key) {
                Node* new_node = new Node(key);
                new_node->next.store(current, std::memory_order_relaxed);
                if (buckets_[index]->next.compare_exchange_strong(current, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
                delete new_node;
                return false;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        Node* new_node = new Node(key);
        if (buckets_[index] == nullptr) {
            buckets_[index] = new_node;
            return true;
        }
        return false;
    }

    bool remove(int key) override {
        int index = hash(key);
        Node* current = buckets_[index];
        Node* prev = nullptr;
        while (current != nullptr) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current);
            } else if (current->key == key) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (prev != nullptr) {
                        prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel);
                    } else {
                        Node* dummy = new Node(0);
                        dummy->next.store(buckets_[index], std::memory_order_relaxed);
                        if (dummy->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
                            delete dummy;
                            return true;
                        }
                        delete dummy;
                        return false;
                    }
                    return true;
                }
                return false;
            }
            prev = current;
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
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

    Node** buckets_;
};