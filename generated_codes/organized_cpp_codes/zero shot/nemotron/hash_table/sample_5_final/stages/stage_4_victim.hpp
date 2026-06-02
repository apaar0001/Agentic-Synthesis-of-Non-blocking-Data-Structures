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
        Node(int v) : val(v), next(nullptr) {}
        Node(int v, Node* n) : val(v), next(n) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    static Node* getNext(Node* n) {
        return get_unmarked_ref(n->next.load(std::memory_order_acquire));
    }

    static size_t hash(int key) {
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(int key, Node** pred_out, Node** curr_out) {
        size_t idx = hash(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);
        while (true) {
            Node* pred = head;
            Node* curr = getNext(pred);
            bool invalid = false;
            while (true) {
                Node* succ = getNext(curr);
                while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* succUnmarked = getNext(curr);
                    if (!pred->next.compare_exchange_strong(curr, succUnmarked,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        invalid = true;
                        break;
                    }
                    curr = succUnmarked;
                    succ = getNext(curr);
                }
                if (invalid) break;
                if (curr->val >= key) {
                    *pred_out = pred;
                    *curr_out = curr;
                    return (curr->val == key && !is_marked_ref(curr->next));
                }
                pred = curr;
                curr = succ;
            }
            if (!invalid) break;
        }
        return false;
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* minSentinel = new Node(INT_MIN);
            Node* maxSentinel = new Node(INT_MAX);
            minSentinel->next.store(maxSentinel, std::memory_order_relaxed);
            buckets[i].store(minSentinel, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* node = buckets[i].load(std::memory_order_acquire);
            while (node) {
                Node* next = get_unmarked_ref(node->next.load(std::memory_order_acquire));
                delete node;
                node = next;
            }
        }
    }

    bool contains(int key) override {
        Node* pred;
        Node* curr;
        find(key, &pred, &curr);
        return (curr->val == key && !is_marked_ref(curr->next));
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            if (find(key, &pred, &curr)) {
                return false;
            }
            Node* node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                                                     std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            // retry if CAS failed
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            if (!find(key, &pred, &curr)) {
                return false;
            }
            Node* succ = getNext(curr);
            if (!curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                continue;
            }
            pred->next.compare_exchange_strong(curr, succ,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
            return true;
        }
    }
};