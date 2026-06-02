#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL);
    }

    Node* head;
    Node* tail;

    // Returns <pred, curr> with curr->val >= key; snips marked nodes along the way
    std::pair<Node*, Node*> find(int key) {
    retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                // curr is logically deleted — snip it
                Node* marked_curr = get_marked_ref(curr);
                if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    goto retry;
                }
                delete curr;
                curr = succ;
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr->val >= key) return {pred, curr};
            pred = curr;
            curr = succ;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_relaxed));
        delete head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr->val < key)
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val == key) return false;
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                return true;
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val != key) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked = get_marked_ref(succ);
            // Logical deletion: mark curr->next
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;
            // Physical deletion (best-effort; find() will clean up on retry)
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }
    }
};
