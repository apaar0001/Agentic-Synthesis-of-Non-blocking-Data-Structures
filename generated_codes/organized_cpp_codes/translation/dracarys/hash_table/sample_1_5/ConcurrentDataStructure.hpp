#pragma once
#include <atomic>
#include <cmath>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        buckets_ = new std::atomic<Node*>[BUCKET_COUNT];
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            buckets_[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (int i = 0; i < BUCKET_COUNT; ++i) {
            while (true) {
                auto node = buckets_[i].load(std::memory_order_acquire);
                if (!node) break;
                auto next = node->next.load(std::memory_order_acquire);
                if (next) {
                    buckets_[i].store(next, std::memory_order_relaxed);
                } else {
                    buckets_[i].store(nullptr, std::memory_order_relaxed);
                    delete node;
                    break;
                }
            }
        }
        delete[] buckets_;
    }

    bool contains(int key) override {
        auto bucket = buckets_[hash(key)].load(std::memory_order_acquire);
        while (bucket) {
            if (is_marked_ref(bucket)) {
                bucket = get_unmarked_ref(bucket);
            } else if (bucket->key == key) {
                return true;
            }
            bucket = bucket->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        auto bucket = buckets_[hash(key)].load(std::memory_order_acquire);
        while (bucket) {
            if (is_marked_ref(bucket)) {
                bucket = get_unmarked_ref(bucket);
            } else if (bucket->key == key) {
                return false;
            }
            bucket = bucket->next.load(std::memory_order_acquire);
        }
        auto new_node = new Node(key);
        while (true) {
            bucket = buckets_[hash(key)].load(std::memory_order_acquire);
            if (is_marked_ref(bucket)) {
                bucket = get_unmarked_ref(bucket);
            }
            if (bucket && bucket->key > key) {
                new_node->next.store(bucket, std::memory_order_relaxed);
                if (buckets_[hash(key)].compare_exchange_strong(bucket, new_node, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                auto prev = bucket;
                while (prev && prev->next.load(std::memory_order_acquire) && prev->next.load(std::memory_order_acquire)->key < key) {
                    prev = prev->next.load(std::memory_order_acquire);
                }
                if (prev) {
                    new_node->next.store(prev->next.load(std::memory_order_acquire), std::memory_order_relaxed);
                    auto expected = prev->next.load(std::memory_order_acquire);
                    if (prev->next.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    new_node->next.store(bucket, std::memory_order_relaxed);
                    if (buckets_[hash(key)].compare_exchange_strong(bucket, new_node, std::memory_order_acq_rel)) {
                        return true;
                    }
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            auto bucket = buckets_[hash(key)].load(std::memory_order_acquire);
            if (is_marked_ref(bucket)) {
                bucket = get_unmarked_ref(bucket);
            }
            if (!bucket) {
                return false;
            }
            if (bucket->key == key) {
                if (is_marked_ref(bucket)) {
                    return false;
                }
                auto next = bucket->next.load(std::memory_order_acquire);
                if (next) {
                    if (buckets_[hash(key)].compare_exchange_strong(bucket, get_marked_ref(next), std::memory_order_acq_rel)) {
                        // Node has been marked
                        while (true) {
                            auto prev = buckets_[hash(key)].load(std::memory_order_acquire);
                            if (is_marked_ref(prev)) {
                                prev = get_unmarked_ref(prev);
                            }
                            if (prev == bucket) {
                                if (buckets_[hash(key)].compare_exchange_strong(bucket, next, std::memory_order_acq_rel)) {
                                    delete bucket;
                                    return true;
                                }
                            } else {
                                break;
                            }
                        }
                    }
                } else {
                    if (buckets_[hash(key)].compare_exchange_strong(bucket, get_marked_ref(bucket), std::memory_order_acq_rel)) {
                        // Node has been marked
                        buckets_[hash(key)].store(nullptr, std::memory_order_relaxed);
                        delete bucket;
                        return true;
                    }
                }
            } else {
                auto prev = bucket;
                while (prev && prev->next.load(std::memory_order_acquire) && prev->next.load(std::memory_order_acquire)->key < key) {
                    prev = prev->next.load(std::memory_order_acquire);
                }
                if (prev && prev->next.load(std::memory_order_acquire) && prev->next.load(std::memory_order_acquire)->key == key) {
                    if (is_marked_ref(prev->next.load(std::memory_order_acquire))) {
                        return false;
                    }
                    auto next = prev->next.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
                    auto expected = prev->next.load(std::memory_order_acquire);
                    if (prev->next.compare_exchange_strong(expected, get_marked_ref(next), std::memory_order_acq_rel)) {
                        // Node has been marked
                        while (true) {
                            auto prev_next = prev->next.load(std::memory_order_acquire);
                            if (is_marked_ref(prev_next)) {
                                prev_next = get_unmarked_ref(prev_next);
                            }
                            if (prev_next == prev->next.load(std::memory_order_acquire)) {
                                auto new_expected = prev->next.load(std::memory_order_acquire);
                                if (prev->next.compare_exchange_strong(new_expected, next, std::memory_order_acq_rel)) {
                                    delete prev->next.load(std::memory_order_acquire);
                                    return true;
                                }
                            } else {
                                break;
                            }
                        }
                    }
                } else {
                    return false;
                }
            }
        }
    }

private:
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int key) : key(key), next(nullptr) {}
    };

    static constexpr int BUCKET_COUNT = 64;

    int hash(int key) {
        return std::abs(key) % BUCKET_COUNT;
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

    std::atomic<Node*>* buckets_;
};