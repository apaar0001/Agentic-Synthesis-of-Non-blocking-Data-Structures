#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstdint>
#include <utility>
#include <climits>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;

    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int k) : key(k), next(nullptr) {}
    };

    std::atomic<Node*> heads[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::size_t hash(int key) const {
        return static_cast<std::size_t>((key % static_cast<int>(BUCKET_COUNT) + BUCKET_COUNT) % BUCKET_COUNT);
    }

    std::pair<Node*, Node*> find(int key, std::size_t idx) {
        while (true) {
            Node* pred = heads[idx].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                bool currMarked = is_marked_ref(curr->next.load(std::memory_order_acquire));
                if (currMarked) {
                    if (pred->next.compare_exchange_strong(
                            curr, succ,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                        continue;
                    } else {
                        break;
                    }
                }
                if (!curr || curr->key >= key) break;
                pred = curr;
                curr = succ;
            }
            Node* predNext = pred->next.load(std::memory_order_acquire);
            if (!is_marked_ref(predNext) && get_unmarked_ref(predNext) == curr) {
                return {pred, curr};
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            heads[i].store(new Node(INT_MIN), std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = heads[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            auto [prev, curr] = find(key, idx);
            if (!curr || curr->key != key) return false;
            return true;
        }
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            auto [prev, curr] = find(key, idx);
            if (curr && curr->key == key) return false;
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (prev->next.compare_exchange_strong(curr, node,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            auto [prev, curr] = find(key, idx);
            if (!curr || curr->key != key) return false;
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                return true;
            }
            Node* succ = get_unmarked_ref(next);
            if (curr->next.compare_exchange_strong(next, get_marked_ref(succ),
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};