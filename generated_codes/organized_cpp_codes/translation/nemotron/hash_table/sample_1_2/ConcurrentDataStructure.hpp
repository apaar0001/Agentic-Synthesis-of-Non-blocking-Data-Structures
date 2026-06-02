#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
        explicit Node(int k) : key(k), next(nullptr) {}
    };
    Node* heads[BUCKET_COUNT];

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::size_t hash(int key) const {
        return static_cast<std::size_t>((key % static_cast<int>(BUCKET_COUNT) + BUCKET_COUNT) % BUCKET_COUNT);
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            heads[i] = new Node(INT_MIN);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = heads[i];
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred = heads[idx];
        while (true) {
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                pred->next.compare_exchange_strong(curr, succ,
                                                 std::memory_order_acq_rel, std::memory_order_acquire);
                curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            }
            if (!curr || curr->key >= key) break;
            pred = curr;
        }
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        return (curr && curr->key == key);
    }

    bool add(int key) override {
        while (true) {
            std::size_t idx = hash(key);
            Node* pred = heads[idx];
            while (true) {
                Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                    Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    pred->next.compare_exchange_strong(curr, succ,
                                                     std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->key >= key) break;
                pred = curr;
            }
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            if (curr && curr->key == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            std::size_t idx = hash(key);
            Node* pred = heads[idx];
            while (true) {
                Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                while (curr && is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                    Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    pred->next.compare_exchange_strong(curr, succ,
                                                     std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->key >= key) break;
                pred = curr;
            }
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            if (!curr || curr->key != key) {
                return false;
            }
            Node* next = curr->next.load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(next)) {
                    return false;
                }
                Node* marked_next = get_marked_ref(next);
                if (curr->next.compare_exchange_strong(next, marked_next,
                                                       std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // Node has been marked
                    break;
                }
                next = curr->next.load(std::memory_order_acquire);
            }
            pred->next.compare_exchange_strong(curr, get_unmarked_ref(next),
                                               std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};