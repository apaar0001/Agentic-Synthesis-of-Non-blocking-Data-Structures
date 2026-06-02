#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 256;

    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int v, Node* n = nullptr)
            : val(v), next(n) {}
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

    size_t hash(int key) const {
        return (static_cast<size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    bool find(Node* head, int key, Node** pred_out, Node** curr_out) {
    retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));

        while (true) {
            Node* succ = curr->next.load(std::memory_order_acquire);

            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);

                Node* expected = curr;
                if (!pred->next.compare_exchange_strong(
                        expected,
                        unmarked_succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    goto retry;
                }

                curr = unmarked_succ;
                succ = curr->next.load(std::memory_order_acquire);
            }

            if (curr->val >= key) {
                *pred_out = pred;
                *curr_out = curr;
                return (curr->val == key);
            }

            pred = curr;
            curr = get_unmarked_ref(succ);
        }
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* tail = new Node(INT_MAX);
            Node* head = new Node(INT_MIN, tail);
            buckets[i].store(head, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);

            while (curr) {
                Node* next = get_unmarked_ref(
                    curr->next.load(std::memory_order_acquire)
                );
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        size_t idx = hash(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);

        Node* curr = get_unmarked_ref(
            head->next.load(std::memory_order_acquire)
        );

        while (curr->val < key) {
            curr = get_unmarked_ref(
                curr->next.load(std::memory_order_acquire)
            );
        }

        Node* succ = curr->next.load(std::memory_order_acquire);

        return (curr->val == key && !is_marked_ref(succ));
    }

    bool add(int key) override {
        size_t idx = hash(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);

        while (true) {
            Node* pred;
            Node* curr;

            if (find(head, key, &pred, &curr)) {
                return false;
            }

            Node* node = new Node(key, curr);

            Node* expected = curr;
            if (pred->next.compare_exchange_strong(
                    expected,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete node;
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);

        while (true) {
            Node* pred;
            Node* curr;

            if (!find(head, key, &pred, &curr)) {
                return false;
            }

            Node* succ = curr->next.load(std::memory_order_acquire);

            if (!is_marked_ref(succ)) {
                Node* marked_succ = get_marked_ref(succ);

                if (curr->next.compare_exchange_strong(
                        succ,
                        marked_succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {

                    Node* expected = curr;
                    pred->next.compare_exchange_strong(
                        expected,
                        succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );

                    return true;
                }
            }
        }
    }
};
