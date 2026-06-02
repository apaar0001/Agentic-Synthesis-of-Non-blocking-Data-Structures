#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <climits>
#include <array>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;

    struct Node {
        int key;
        std::atomic<Node*> next;
        explicit Node(int k) : key(k), next(nullptr) {}
    };

    std::array<Node*, BUCKET_COUNT> heads_;
    std::array<Node*, BUCKET_COUNT> tails_;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    static std::size_t hash(int key) {
        return static_cast<std::size_t>(key) % BUCKET_COUNT;
    }

    bool find(int key, Node*& pred, Node*& curr) {
        std::size_t idx = hash(key);
        while (true) {
            pred = heads_[idx];
            Node* curr_candidate = nullptr;
            while (true) {
                Node* next_raw = pred->next.load(std::memory_order_acquire);
                if (is_marked_ref(next_raw)) {
                    Node* next_unmarked = get_unmarked_ref(next_raw);
                    Node* succ = get_unmarked_ref(next_unmarked->next.load(std::memory_order_acquire));
                    if (pred->next.compare_exchange_strong(next_raw, get_unmarked_ref(succ),
                                                          std::memory_order_release, std::memory_order_relaxed)) {
                        continue;
                    } else {
                        break;
                    }
                }
                curr_candidate = get_unmarked_ref(next_raw);
                if (!curr_candidate || curr_candidate->key >= key) {
                    break;
                }
                pred = curr_candidate;
            }
            Node* curr_raw = pred->next.load(std::memory_order_acquire);
            if (is_marked_ref(curr_raw)) {
                continue;
            }
            if (get_unmarked_ref(curr_candidate) == get_unmarked_ref(curr_raw)) {
                curr = curr_candidate;
                return true;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            heads_[i] = new Node(INT_MIN);
            tails_[i] = new Node(INT_MAX);
            heads_[i]->next.store(get_unmarked_ref(tails_[i]), std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node