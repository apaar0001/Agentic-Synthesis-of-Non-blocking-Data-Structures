#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 16;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static size_t hash(int key) {
        return (static_cast<uint32_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    void find(size_t idx, int key, Node*& pred, Node*& curr) {
        Node* head = buckets[idx].load(std::memory_order_acquire);
        while (true) {
            pred = head;
            bool restart = false;
            while (true) {
                Node* nextRaw = pred->next.load(std::memory_order_acquire);
                while (is_marked_ref(nextRaw)) {
                    Node* succ = get_unmarked_ref(nextRaw);
                    if (!pred->next.compare_exchange_strong(nextRaw, succ,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_relaxed)) {
                        restart = true;
                        break;
                    }
                    nextRaw = pred->next.load(std::memory_order_acquire);
                }
                if (restart) break;
                curr = get_unmarked_ref(nextRaw);
                if (curr->val >= key) {
                    return;
                }
                pred = curr;
            }
            if (restart) {
                continue;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* minNode = new Node(INT_MIN);
            Node* maxNode = new Node(INT_MAX);
            minNode->next.store(maxNode, std::memory_order_relaxed);
            buckets[i].store(minNode, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = buckets[i].load(std::memory_order_acquire);
            while (cur != nullptr) {
                Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = next;
            }
        }
    }

    bool contains(int key) override {
        if (key == INT_MIN || key == INT_MAX) return false;
        size_t idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(idx, key, pred, curr);
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        if (key == INT_MIN || key == INT_MAX) return false;
        while (true) {
            size_t idx = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(idx, key, pred, curr);
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                                                       std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) override {
        if (key == INT_MIN || key == INT_MAX) return false;
        while (true) {
            size_t idx = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(idx, key, pred, curr);
            if (curr->val != key) {
                return false;
            }
            Node* next = curr->next.load(std::memory_order_acquire);
            while (!is_marked_ref(next)) {
                Node* markedNext = get_marked_ref(next);
                if (curr->next.compare_exchange_weak(next, markedNext,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed)) {
                                                         std::this_thread::sleep_for(std::chrono::seconds(5));
                    break;
                }
                next = curr->next.load(std::memory_order_acquire);
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (pred->next.compare_exchange_strong(curr, succ,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                                                       std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }
};