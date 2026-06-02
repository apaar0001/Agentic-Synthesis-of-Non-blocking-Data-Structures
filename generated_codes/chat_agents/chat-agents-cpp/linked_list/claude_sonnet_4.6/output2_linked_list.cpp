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

    std::atomic<Node*> head_atomic;

    Node* sentinel_head;
    Node* sentinel_tail;

    bool search(int key, Node*& out_pred, Node*& out_curr) {
    again:
        out_pred = sentinel_head;
        out_curr = get_unmarked_ref(out_pred->next.load(std::memory_order_acquire));
        while (true) {
            if (!out_curr) return false;
            Node* raw_next = out_curr->next.load(std::memory_order_acquire);
            Node* out_succ = get_unmarked_ref(raw_next);
            bool marked = is_marked_ref(raw_next);
            if (marked) {
                if (!out_pred->next.compare_exchange_strong(out_curr, out_succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed))
                    goto again;
                delete out_curr;
                out_curr = out_succ;
            } else {
                if (out_curr->val >= key) return out_curr->val == key;
                out_pred = out_curr;
                out_curr = out_succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        sentinel_head = new Node(INT_MIN);
        sentinel_tail = new Node(INT_MAX);
        sentinel_head->next.store(sentinel_tail, std::memory_order_relaxed);
        head_atomic.store(sentinel_head, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* n = get_unmarked_ref(sentinel_head->next.load(std::memory_order_relaxed));
        delete sentinel_head;
        while (n) {
            Node* nx = get_unmarked_ref(n->next.load(std::memory_order_relaxed));
            delete n;
            n = nx;
        }
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(sentinel_head->next.load(std::memory_order_acquire));
        while (curr && curr->val < key)
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return curr && curr->val == key
            && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        Node* pred, *curr;
        while (true) {
            bool found = search(key, pred, curr);
            if (found) return false;
            Node* nd = new Node(key);
            nd->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, nd,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                return true;
            delete nd;
        }
    }

    bool remove(int key) override {
        Node* pred, *curr;
        while (true) {
            bool found = search(key, pred, curr);
            if (!found) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }
    }
};
