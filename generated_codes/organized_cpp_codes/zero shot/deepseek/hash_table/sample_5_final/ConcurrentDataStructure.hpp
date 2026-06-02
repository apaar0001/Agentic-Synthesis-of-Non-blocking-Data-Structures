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

    bool find(int key, Node*& pred, Node*& curr, Node* head) const {
        Node* succ;
        while (true) {
            pred = head;
            curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                if (curr == nullptr) {
                    return false;
                }
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                while (is_marked_ref(curr->next.load(std::memory_order_relaxed))) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    if (!pred->next.compare_exchange_strong(curr, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    if (curr == nullptr) {
                        return false;
                    }
                    succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                }
                if (curr == nullptr || curr->val >= key) {
                    return curr != nullptr && curr->val == key;
                }
                pred = curr;
                curr = succ;
            }
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
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr != nullptr && curr->val < key) {
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (curr != nullptr && is_marked_ref(curr->next.load(std::memory_order_relaxed))) {
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
        }
        return curr != nullptr && curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        std::size_t idx = hash(key);
        Node* new_node = new Node(key);
        while (true) {
            Node* pred;
            Node* curr;
            Node* head = buckets[idx].load(std::memory_order_acquire);
            if (find(key, pred, curr, head)) {
                delete new_node;
                return false;
            }
            new_node->next.store(curr, std::memory_order_relaxed);
            Node* unmarked_pred = get_unmarked_ref(pred);
            if (unmarked_pred->next.compare_exchange_strong(curr, new_node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        std::size_t idx = hash(key);
        while (true) {
            Node* pred;
            Node* curr;
            Node* head = buckets[idx].load(std::memory_order_acquire);
            if (!find(key, pred, curr, head)) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_succ,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            Node* unmarked_pred = get_unmarked_ref(pred);
            unmarked_pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};