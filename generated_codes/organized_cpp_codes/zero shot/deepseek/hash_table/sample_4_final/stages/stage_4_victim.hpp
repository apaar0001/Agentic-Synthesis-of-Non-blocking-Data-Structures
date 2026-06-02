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
    static constexpr std::size_t BUCKET_COUNT = 1024;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int v, Node* n = nullptr) : val(v), next(n) {}
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

    std::size_t hash(int key) const {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(std::atomic<Node*>* head, int key, Node** pred, Node** curr) {
        Node* curr_next;
        Node* pred_next;
        bool retry;

        while (true) {
            retry = false;
            *pred = head->load(std::memory_order_acquire);
            *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));

            while (true) {
                if (get_unmarked_ref(*curr) == nullptr) {
                    return false;
                }

                curr_next = get_unmarked_ref(*curr)->next.load(std::memory_order_acquire);

                if (get_unmarked_ref(*curr)->next.load(std::memory_order_relaxed) != curr_next) {
                    retry = true;
                    break;
                }

                if (!is_marked_ref(curr_next)) {
                    if (get_unmarked_ref(*curr)->val >= key) {
                        return get_unmarked_ref(*curr)->val == key;
                    }
                    *pred = get_unmarked_ref(*curr);
                    *curr = get_unmarked_ref(curr_next);
                } else {
                    pred_next = get_unmarked_ref(*curr);
                    if ((*pred)->next.compare_exchange_strong(pred_next, get_unmarked_ref(curr_next), std::memory_order_acq_rel)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                        delete get_unmarked_ref(*curr);
                        *curr = get_unmarked_ref(curr_next);
                    } else {
                        retry = true;
                        break;
                    }
                }
            }

            if (retry) continue;
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr != nullptr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* pred;
        Node* curr;
        return find(&buckets[idx], key, &pred, &curr);
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        Node* new_node = nullptr;

        while (true) {
            Node* pred;
            Node* curr;

            if (find(&buckets[idx], key, &pred, &curr)) {
                if (new_node != nullptr) delete new_node;
                return false;
            }

            if (new_node == nullptr) {
                new_node = new Node(key);
            }
            new_node->next.store(curr, std::memory_order_relaxed);

            Node* unmarked_curr = get_unmarked_ref(curr);
            if (pred->next.compare_exchange_strong(unmarked_curr, new_node, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);

        while (true) {
            Node* pred;
            Node* curr;

            if (!find(&buckets[idx], key, &pred, &curr)) {
                return false;
            }

            Node* curr_next = get_unmarked_ref(curr)->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(curr_next);

            if (!get_unmarked_ref(curr)->next.compare_exchange_strong(curr_next, marked_next, std::memory_order_acq_rel)) {
                continue;
            }

            Node* unmarked_curr = get_unmarked_ref(curr);
            if (pred->next.compare_exchange_strong(unmarked_curr, curr_next, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                delete get_unmarked_ref(curr);
            } else {
                find(&buckets[idx], key, &pred, &curr);
            }
            return true;
        }
    }
};