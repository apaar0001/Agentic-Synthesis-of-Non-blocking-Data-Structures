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

    // Simple pause for contention backoff (no OS sleep — stays lock-free)
    static void backoff(int& delay) {
        for (int i = 0; i < delay; ++i)
            std::atomic_signal_fence(std::memory_order_seq_cst);
        if (delay < 1024) delay *= 2;
    }

    std::pair<Node*, Node*> find(int key) {
        int delay = 1;
    retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            Node* raw  = curr->next.load(std::memory_order_acquire);
            Node* succ = get_unmarked_ref(raw);
            while (is_marked_ref(raw)) {
                if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    backoff(delay);
                    goto retry;
                }
                delete curr;
                curr = succ;
                raw  = curr->next.load(std::memory_order_acquire);
                succ = get_unmarked_ref(raw);
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
        Node* n = get_unmarked_ref(head->next.load(std::memory_order_relaxed));
        delete head;
        while (n) {
            Node* nx = get_unmarked_ref(n->next.load(std::memory_order_relaxed));
            delete n;
            n = nx;
        }
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr->val < key)
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        int delay = 1;
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val == key) return false;
            Node* nd = new Node(key);
            nd->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, nd,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                return true;
            delete nd;
            backoff(delay);
        }
    }

    bool remove(int key) override {
        int delay = 1;
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val != key) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                backoff(delay);
                continue;
            }
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }
    }
};
