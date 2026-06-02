#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int v) : val(v), next(nullptr) {}
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

    Node* create_sentinel_list() const {
        Node* head = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
        return head;
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(create_sentinel_list(), std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_relaxed);
            while (curr) {
                Node* next = curr->next.load(std::memory_order_relaxed);
                next = get_unmarked_ref(next);
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* curr = buckets[idx].load(std::memory_order_acquire);
        curr = get_unmarked_ref(curr);

        while (curr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            while (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
                curr = curr->next.load(std::memory_order_acquire);
                curr = get_unmarked_ref(curr);
                if (!curr) break;
                next = curr->next.load(std::memory_order_acquire);
            }

            if (!curr) break;

            int curr_val = curr->val;
            if (curr_val == key) {
                return !is_marked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr_val > key) {
                break;
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
                if (!curr) return false;

                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = unmarked_succ;
                    if (!curr) return false;
                    succ = curr->next.load(std::memory_order_acquire);
                }

                int curr_val = curr->val;
                if (curr_val == key) {
                    return false;
                }
                if (curr_val > key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }

            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);

            if (pred->next.compare_exchange_strong(curr, new_node,
                    std::memory_order_acq_rel)) {
                return true;
            }

            delete new_node;
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
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                            std::memory_order_acq_rel)) {
                        break;
                    }
                    curr = unmarked_succ;
                    if (!curr) return false;
                    succ = curr->next.load(std::memory_order_acquire);
                }

                int curr_val = curr->val;
                if (curr_val == key) {
                    Node* marked_succ = get_marked_ref(succ);
                    if (!curr->next.compare_exchange_strong(succ, marked_succ,
                            std::memory_order_acq_rel)) {
                        continue;
                    }

                    pred->next.compare_exchange_strong(curr, succ,
                            std::memory_order_acq_rel);
                    return true;
                }
                if (curr_val > key) {
                    return false;
                }
                pred = curr;
                curr = succ;
            }
        }
    }
};