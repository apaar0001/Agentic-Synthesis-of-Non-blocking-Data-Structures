#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

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

    bool search(int key, Node** pred, Node** curr, Node* head) const {
        Node* pred_node;
        Node* curr_node;
        Node* succ_node;

    retry:
        pred_node = head;
        curr_node = pred_node->next.load(std::memory_order_acquire);
        while (true) {
            if (curr_node == nullptr) {
                *pred = pred_node;
                *curr = curr_node;
                return false;
            }
            succ_node = curr_node->next.load(std::memory_order_acquire);
            while (is_marked_ref(succ_node)) {
                Node* unmarked_succ = get_unmarked_ref(succ_node);
                if (!pred_node->next.compare_exchange_strong(curr_node, unmarked_succ,
                                                             std::memory_order_acq_rel)) {
                    goto retry;
                }
                curr_node = unmarked_succ;
                if (curr_node == nullptr) {
                    *pred = pred_node;
                    *curr = curr_node;
                    return false;
                }
                succ_node = curr_node->next.load(std::memory_order_acquire);
            }
            if (curr_node->val >= key) {
                *pred = pred_node;
                *curr = curr_node;
                return curr_node->val == key;
            }
            pred_node = curr_node;
            curr_node = succ_node;
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
        Node* head = buckets[idx].load(std::memory_order_acquire);
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return true;
            }
            if (unmarked_curr->val > key) {
                return false;
            }
            curr = unmarked_curr->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        Node* new_node = new Node(key);
        Node* pred;
        Node* curr;

        while (true) {
            if (search(key, &pred, &curr, buckets[idx].load(std::memory_order_acquire))) {
                delete new_node;
                return false;
            }
            new_node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, new_node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        Node* pred;
        Node* curr;
        Node* succ;

        while (true) {
            if (!search(key, &pred, &curr, buckets[idx].load(std::memory_order_acquire))) {
                return false;
            }
            succ = curr->next.load(std::memory_order_acquire);
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_succ,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                continue;
            }
            if (pred->next.compare_exchange_strong(curr, succ,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                delete curr;
            } else {
                search(key, &pred, &curr, buckets[idx].load(std::memory_order_acquire));
            }
            return true;
        }
    }
};