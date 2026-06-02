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
    };

    ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i] = nullptr;
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* current = buckets[i].load(std::memory_order_acquire);
            while (current != nullptr) {
                Node* next = current->next.load(std::memory_order_acquire);
                delete current;
                current = next;
            }
        }
    }

    bool contains(int key) override {
        int index = hash(key);
        Node* current = buckets[index].load(std::memory_order_acquire);
        while (current != nullptr) {
            if (is_marked_ref(current->next.load(std::memory_order_acquire))) {
                current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            } else if (current->key == key) {
                return true;
            }
            current = current->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        int index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* prev = nullptr;
            while (current != nullptr) {
                if (is_marked_ref(current->next.load(std::memory_order_acquire))) {
                    current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
                } else if (current->key == key) {
                    return false;
                } else if (current->key > key) {
                    Node* newNode = new Node{key, current};
                    if (prev != nullptr) {
                        Node* expected = current;
                        if (prev->next.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                    } else {
                        Node* expected = current;
                        if (buckets[index].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                    }
                    break;
                }
                prev = current;
                current = current->next.load(std::memory_order_acquire);
            }
            if (current == nullptr) {
                Node* newNode = new Node{key, nullptr};
                if (prev != nullptr) {
                    Node* expected = nullptr;
                    if (prev->next.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    Node* expected = nullptr;
                    if (buckets[index].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    bool remove(int key) override {
        int index = hash(key);
        while (true) {
            Node* current = buckets[index].load(std::memory_order_acquire);
            Node* prev = nullptr;
            while (current != nullptr) {
                if (is_marked_ref(current->next.load(std::memory_order_acquire))) {
                    current = get_unmarked_ref(current->next.load(std::memory_order_acquire));
                } else if (current->key == key) {
                    if (is_marked_ref(current)) {
                        return false;
                    }
                    Node* next = current->next.load(std::memory_order_acquire);
                    if (prev != nullptr) {
                        Node* expected = current;
                        if (prev->next.compare_exchange_strong(expected, get_marked_ref(next), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                    } else {
                        Node* expected = current;
                        if (buckets[index].compare_exchange_strong(expected, get_marked_ref(next), std::memory_order_acq_rel)) {
                            // Node has been marked
                            return true;
                        }
                    }
                    break;
                }
                prev = current;
                current = current->next.load(std::memory_order_acquire);
            }
            return false;
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

    std::atomic<Node*> buckets[BUCKET_COUNT];
};