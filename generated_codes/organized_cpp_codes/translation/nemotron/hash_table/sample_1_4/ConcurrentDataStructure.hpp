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
    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    std::size_t hash(int key) const {
        return static_cast<std::size_t>(static_cast<unsigned int>(key) & (BUCKET_COUNT - 1));
    }

    bool find(int key, std::size_t idx, Node*& pred, Node*& curr) {
        Node* head = buckets[idx].load(std::memory_order_acquire);
        retry:
        pred = head;
        curr = pred->next.load(std::memory_order_acquire);
        while (true) {
            while (is_marked_ref(curr)) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (!pred->next.compare_exchange_strong(curr, succ,
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    goto retry;
                }
                curr = succ;
            }
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                if (!curr->next.compare_exchange_strong(succ, get_unmarked_ref(succ),
                                                        std::memory_order_release, std::memory_order_relaxed)) {
                    goto retry;
                }
                continue;
            }
            if (curr->key >= key) {
                break;
            }
            pred = curr;
            curr = succ;
        }
        return (curr->key == key);
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_release);
            tail->next.store(nullptr, std::memory_order_release);
            buckets[i].store(head, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = buckets[i].load(std::memory_order_acquire);
            while (cur) {
                Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred, *curr;
        find(key, idx, pred, curr);
        return (curr->key == key);
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred, *curr;
            if (find(key, idx, pred, curr)) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_release, std::memory_order_relaxed)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred, *curr;
            if (!find(key, idx, pred, curr)) {
                return false;
            }
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_raw)) {
                continue;
            }
            Node* marked_next = get_marked_ref(curr);
            if (curr->next.compare_exchange_strong(next_raw, marked_next,
                                                   std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};