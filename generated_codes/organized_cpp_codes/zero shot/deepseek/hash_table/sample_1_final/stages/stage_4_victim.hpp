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
    static constexpr std::size_t BUCKET_COUNT = 256;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    static std::size_t hash(int key) {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    std::atomic<Node*> buckets[BUCKET_COUNT];

    void init_bucket(std::size_t idx) {
        Node* tail = new Node(INT_MAX);
        Node* head = new Node(INT_MIN, tail);
        buckets[idx].store(head, std::memory_order_relaxed);
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            init_bucket(i);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                Node* next = unmarked->next.load(std::memory_order_relaxed);
                next = get_unmarked_ref(next);
                delete unmarked;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* curr = buckets[idx].load(std::memory_order_acquire);
        curr = get_unmarked_ref(curr);

        while (curr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* next = unmarked_curr->next.load(std::memory_order_acquire);

            while (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
                next = next->next.load(std::memory_order_acquire);
            }

            if (unmarked_curr->val == key) {
                return !is_marked_ref(unmarked_curr->next.load(std::memory_order_acquire));
            }
            if (unmarked_curr->val > key) {
                return false;
            }
            curr = next;
        }
        return false;
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* head = buckets[idx].load(std::memory_order_acquire);
            Node* pred = get_unmarked_ref(head);
            Node* curr = pred->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(curr);

            while (true) {
                if (!curr) break;
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    delete unmarked_curr;
                    curr = unmarked_succ;
                    if (!curr) break;
                    succ = curr->next.load(std::memory_order_acquire);
                }

                if (!curr || curr->val > key) {
                    if (pred->val == key) {
                        return false;
                    }
                    Node* new_node = new Node(key, curr);
                    if (pred->next.compare_exchange_strong(curr, new_node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                                std::this_thread::sleep_for(std::chrono::seconds(5));
                        return true;
                    }
                    delete new_node;
                    goto retry;
                }
                if (curr->val == key) {
                    return false;
                }
                pred = curr;
                curr = succ;
                curr = get_unmarked_ref(curr);
            }
            retry:;
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* head = buckets[idx].load(std::memory_order_acquire);
            Node* pred = get_unmarked_ref(head);
            Node* curr = pred->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(curr);

            while (true) {
                if (!curr) return false;
                Node* succ = curr->next.load(std::memory_order_acquire);

                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry_remove;
                    }
                    delete unmarked_curr;
                    curr = unmarked_succ;
                    if (!curr) return false;
                    succ = curr->next.load(std::memory_order_acquire);
                }

                if (curr->val > key) {
                    return false;
                }
                if (curr->val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (!curr->next.compare_exchange_strong(succ, marked_succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        continue;
                    }
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (!pred->next.compare_exchange_strong(curr, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        curr->next.store(succ, std::memory_order_release);
                    } else {
                        delete unmarked_curr;
                    }
                    return true;
                }
                pred = curr;
                curr = succ;
                curr = get_unmarked_ref(curr);
            }
            retry_remove:;
        }
    }
};