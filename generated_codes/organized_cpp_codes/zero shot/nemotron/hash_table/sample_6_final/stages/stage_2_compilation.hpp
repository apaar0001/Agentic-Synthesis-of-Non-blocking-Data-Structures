#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* getUnmarked(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* getMarked(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static bool isMarked(Node* p) {
        return reinterpret_cast<uintptr_t>(p) & 1ULL;
    }

    static size_t hash(int key) {
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    Node* getBucketHead(size_t idx) const {
        return getUnmarked(buckets[idx].load(std::memory_order_acquire));
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(getUnmarked(tail), std::memory_order_relaxed);
            tail->next.store(nullptr, std::memory_order_relaxed);
            buckets[i].store(getUnmarked(head), std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* node = getBucketHead(i);
            while (node) {
                Node* next = getUnmarked(node->next.load(std::memory_order_relaxed));
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = getBucketHead(idx);
            Node* curr = getUnmarked(pred->next.load(std::memory_order_acquire));
            while (true) {
                // help remove marked nodes
                while (curr && isMarked(pred->next.load(std::memory_order_acquire))) {
                    Node* succ = getUnmarked(curr->next.load(std::memory_order_acquire));
                    pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = getUnmarked(pred->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->val >= key) break;
                pred = curr;
                curr = getUnmarked(curr->next.load(std::memory_order_acquire));
            }
            if (curr && curr->val == key && !isMarked(pred->next.load(std::memory_order_acquire))) {
                return true;
            }
            // if we fell off the list or found a larger key, not present
            return false;
        }
    }

    bool add(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = getBucketHead(idx);
            Node* curr = getUnmarked(pred->next.load(std::memory_order_acquire));
            while (true) {
                // help remove marked nodes
                while (curr && isMarked(pred->next.load(std::memory_order_acquire))) {
                    Node* succ = getUnmarked(curr->next.load(std::memory_order_acquire));
                    pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = getUnmarked(pred->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->val >= key) break;
                pred = curr;
                curr = getUnmarked(curr->next.load(std::memory_order_acquire));
            }
            if (curr && curr->val == key && !isMarked(pred->next.load(std::memory_order_acquire))) {
                return false; // already present
            }
            Node* node = new Node(key);
            node->next.store(getUnmarked(curr), std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(getUnmarked(curr), getUnmarked(node),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            // CAS failed, retry
            delete node;
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* pred = getBucketHead(idx);
            Node* curr = getUnmarked(pred->next.load(std::memory_order_acquire));
            while (true) {
                // help remove marked nodes
                while (curr && isMarked(pred->next.load(std::memory_order_acquire))) {
                    Node* succ = getUnmarked(curr->next.load(std::memory_order_acquire));
                    pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = getUnmarked(pred->next.load(std::memory_order_acquire));
                }
                if (!curr || curr->val >= key) break;
                pred = curr;
                curr = getUnmarked(curr->next.load(std::memory_order_acquire));
            }
            if (!(curr && curr->val == key && !isMarked(pred->next.load(std::memory_order_acquire)))) {
                return false; // not present
            }
            // logical removal: mark curr's next pointer
            Node* succ = getUnmarked(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, getMarked(succ),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // failed to mark, retry
                continue;
            }
            // physical removal: unlink curr from pred
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};