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

    // Fully iterative — no goto, uses a restart flag
    bool find(int key, Node*& out_pred, Node*& out_curr) {
        while (true) {
            bool restart = false;
            out_pred = head;
            out_curr = get_unmarked_ref(out_pred->next.load(std::memory_order_acquire));

            while (!restart) {
                Node* raw  = out_curr->next.load(std::memory_order_acquire);
                Node* succ = get_unmarked_ref(raw);

                if (is_marked_ref(raw)) {
                    // Attempt physical removal
                    if (!out_pred->next.compare_exchange_strong(out_curr, succ,
                            std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        restart = true;
                        break;
                    }
                    delete out_curr;
                    out_curr = succ;
                    continue;
                }

                if (out_curr->val >= key) {
                    return out_curr->val == key;
                }
                out_pred = out_curr;
                out_curr = succ;
            }
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

    // contains() is a lock-free read-only traversal — no CAS needed
    // Linearization point: the moment curr->val == key is observed with unmarked next
    bool contains(int key) override {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr->val < key)
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return curr->val == key
            && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred, *curr;
            bool found = find(key, pred, curr);
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
        while (true) {
            Node* pred, *curr;
            bool found = find(key, pred, curr);
            if (!found) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* marked = get_marked_ref(succ);
            // Phase 1: logical deletion
            if (!curr->next.compare_exchange_strong(succ, marked,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;
            // Phase 2: physical unlink (best-effort; find() handles missed cases)
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }
    }
};
