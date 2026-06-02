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
    static constexpr size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node() : val(0), next(nullptr) {}
        explicit Node(int v) : val(v), next(nullptr) {}
    };

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
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    std::atomic<Node*> buckets[BUCKET_COUNT];

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_release);
            buckets[i].store(head, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* prev = buckets[i].load(std::memory_order_acquire);
            while (prev) {
                Node* next = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
                delete prev;
                prev = next;
            }
        }
    }

    bool contains(int key) override {
        size_t b = hash(key);
        Node* prev = buckets[b].load(std::memory_order_acquire);
        Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
        while (true) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_raw)) {
                Node* succ = get_unmarked_ref(next_raw);
                prev->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire);
                curr = succ;
                continue;
            }
            Node* next = get_unmarked_ref(next_raw);
            if (curr->val >= key) break;
            prev = curr;
            curr = next;
        }
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        size_t b = hash(key);
        while (true) {
            Node* prev = buckets[b].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            while (true) {
                Node* next_raw = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next_raw)) {
                    Node* succ = get_unmarked_ref(next_raw);
                    prev->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = succ;
                    continue;
                }
                Node* next = get_unmarked_ref(next_raw);
                if (curr->val >= key) break;
                prev = curr;
                curr = next;
            }
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (prev->next.compare_exchange_strong(curr, node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        size_t b = hash(key);
        while (true) {
            Node* prev = buckets[b].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(prev->next.load(std::memory_order_acquire));
            while (true) {
                Node* next_raw = curr->next.load(std::memory_order_acquire);
                if (is_marked_ref(next_raw)) {
                    Node* succ = get_unmarked_ref(next_raw);
                    prev->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = succ;
                    continue;
                }
                Node* next = get_unmarked_ref(next_raw);
                if (curr->val >= key) break;
                prev = curr;
                curr = next;
            }
            if (curr->val != key) {
                return false;
            }
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            Node* succ = get_unmarked_ref(next_raw);
            if (curr->next.compare_exchange_strong(next_raw, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }
};