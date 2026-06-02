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

        Node(int key, Node* next) : key(key), next(next) {}
    };

    static bool is_marked_ref(uintptr_t ref) {
        return (ref & 1) == 1;
    }

    static uintptr_t get_unmarked_ref(uintptr_t ref) {
        return ref & ~1;
    }

    static uintptr_t get_marked_ref(uintptr_t ref) {
        return ref | 1;
    }

    ConcurrentDataStructure() : buckets_(new std::atomic<Node*>[BUCKET_COUNT]()) {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets_[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            Node* node = buckets_[i].load(std::memory_order_acquire);
            while (node) {
                Node* next = node->next.load(std::memory_order_acquire);
                delete node;
                node = next;
            }
        }
        delete[] buckets_;
    }

    bool contains(int key) override {
        int index = hash(key);
        Node* node = buckets_[index].load(std::memory_order_acquire);
        while (node) {
            if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
                node = node->next.load(std::memory_order_acquire);
                continue;
            }
            if (node->key == key) {
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
            while (node) {
                if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
                    node = node->next.load(std::memory_order_acquire);
                    continue;
                }
                if (node->key == key) {
                    return false;
                }
                node = node->next.load(std::memory_order_acquire);
            }
            Node* newNode = new Node(key, nullptr);
            Node* prev = nullptr;
            Node* curr = buckets_[index].load(std::memory_order_acquire);
            while (curr) {
                if (is_marked_ref(reinterpret_cast<uintptr_t>(curr))) {
                    curr = curr->next.load(std::memory_order_acquire);
                    continue;
                }
                if (curr->key == key) {
                    delete newNode;
                    return false;
                }
                prev = curr;
                curr = curr->next.load(std::memory_order_acquire);
            }
            newNode->next.store(nullptr, std::memory_order_relaxed);
            if (prev) {
                while (true) {
                    Node* next = prev->next.load(std::memory_order_acquire);
                    if (next == curr) {
                        if (prev->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                    } else {
                        curr = next;
                        prev = nullptr;
                        break;
                    }
                }
            } else {
                while (true) {
                    Node* next = buckets_[index].load(std::memory_order_acquire);
                    if (next == curr) {
                        if (buckets_[index].compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                    } else {
                        curr = next;
                        prev = nullptr;
                        break;
                    }
                }
            }
        }
    }

    bool remove(int key) override {
        int index = hash(key);
        while (true) {
            Node* prev = nullptr;
            Node* node = buckets_[index].load(std::memory_order_acquire);
            while (node) {
                if (is_marked_ref(reinterpret_cast<uintptr_t>(node))) {
                    node = node->next.load(std::memory_order_acquire);
                    continue;
                }
                if (node->key == key) {
                    break;
                }
                prev = node;
                node = node->next.load(std::memory_order_acquire);
            }
            if (!node || node->key != key) {
                return false;
            }
            Node* next = node->next.load(std::memory_order_acquire);
            if (is_marked_ref(reinterpret_cast<uintptr_t>(next))) {
                return false;
            }
            if (prev) {
                while (true) {
                    Node* temp = prev->next.load(std::memory_order_acquire);
                    if (temp == node) {
                        if (prev->next.compare_exchange_strong(node, next, std::memory_order_acq_rel)) {
                            // Node has been marked
                            delete node;
                            return true;
                        }
                    } else {
                        prev = nullptr;
                        node = temp;
                        break;
                    }
                }
            } else {
                while (true) {
                    Node* temp = buckets_[index].load(std::memory_order_acquire);
                    if (temp == node) {
                        if (buckets_[index].compare_exchange_strong(node, next, std::memory_order_acq_rel)) {
                            // Node has been marked
                            delete node;
                            return true;
                        }
                    } else {
                        prev = nullptr;
                        node = temp;
                        break;
                    }
                }
            }
        }
    }

private:
    int hash(int key) {
        return key % BUCKET_COUNT;
    }

    std::atomic<Node*>* buckets_;
};