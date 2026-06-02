#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        for (auto& bucket : buckets_) {
            bucket = new Node(INT_MIN);
        }
    }

    bool contains(int key) override {
        auto bucket = buckets_[hash_(key)];
        while (true) {
            auto node = bucket->next_.load(std::memory_order_acquire);
            if (is_marked_ref(node)) {
                continue;
            }
            if (get_unmarked_ref(node)->key_ == key) {
                return true;
            }
            if (get_unmarked_ref(node)->key_ > key) {
                return false;
            }
            bucket = get_unmarked_ref(node);
        }
    }

    bool add(int key) override {
        auto bucket = buckets_[hash_(key)];
        while (true) {
            auto node = bucket->next_.load(std::memory_order_acquire);
            if (is_marked_ref(node)) {
                continue;
            }
            if (get_unmarked_ref(node)->key_ == key) {
                return false;
            }
            if (get_unmarked_ref(node)->key_ > key) {
                auto new_node = new Node(key);
                new_node->next_ = node;
                if (bucket->next_.compare_exchange_strong(node, new_node, std::memory_order_acq_rel)) {
                    return true;
                } else {
                    // retry if CAS fails
                    continue;
                }
            } else {
                bucket = get_unmarked_ref(node);
            }
        }
    }

    bool remove(int key) override {
        auto bucket = buckets_[hash_(key)];
        while (true) {
            auto node = bucket->next_.load(std::memory_order_acquire);
            if (is_marked_ref(node)) {
                continue;
            }
            if (get_unmarked_ref(node)->key_ == key) {
                auto next = get_unmarked_ref(node)->next_.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    return false;
                }
                auto marked_next = get_marked_ref(next);
                if (get_unmarked_ref(node)->next_.compare_exchange_strong(next, marked_next, std::memory_order_acq_rel)) {
                    // Node has been marked
                    while (true) {
                        auto next_next = get_unmarked_ref(node)->next_.load(std::memory_order_acquire);
                        if (is_marked_ref(next_next)) {
                            continue;
                        }
                        if (bucket->next_.compare_exchange_strong(node, next_next, std::memory_order_acq_rel)) {
                            delete get_unmarked_ref(node);
                            return true;
                        } else {
                            // retry if CAS fails
                            node = bucket->next_.load(std::memory_order_acquire);
                        }
                    }
                }
            } else if (get_unmarked_ref(node)->key_ > key) {
                return false;
            } else {
                bucket = get_unmarked_ref(node);
            }
        }
    }

private:
    struct Node {
        int key_;
        std::atomic<Node*> next_;
        Node(int key) : key_(key), next_(nullptr) {}
    };

    static constexpr int BUCKET_COUNT = 64;
    Node* buckets_[BUCKET_COUNT];

    int hash_(int key) {
        return key % BUCKET_COUNT;
    }

    static bool is_marked_ref(Node* node) {
        return reinterpret_cast<uintptr_t>(node) & 1;
    }

    static Node* get_unmarked_ref(Node* node) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(node) & ~1);
    }

    static Node* get_marked_ref(Node* node) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(node) | 1);
    }
};