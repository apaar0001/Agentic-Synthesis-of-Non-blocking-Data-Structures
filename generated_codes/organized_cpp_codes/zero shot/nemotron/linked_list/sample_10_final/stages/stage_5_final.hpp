#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static Node* get_next(Node* n) {
        return get_unmarked_ref(n->next.load(std::memory_order_acquire));
    }

    Node* head;

    void find(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head;
            curr = get_next(pred);
            while (curr->val < key) {
                pred = curr;
                curr = get_next(curr);
            }
            // Ensure curr is not marked (should be unmarked by get_next)
            if (!is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return;
            }
            // If curr is marked, help remove it and retry
            Node* succ = get_next(curr);
            pred->next.compare_exchange_strong(curr, succ,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
        }
    }

public:
    ConcurrentDataStructure() {
        Node* min_sentinel = new Node(INT_MIN);
        Node* max_sentinel = new Node(INT_MAX);
        min_sentinel->next.store(max_sentinel, std::memory_order_release);
        head = min_sentinel;
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = get_next(head);
        while (curr->val < key) {
            curr = get_next(curr);
        }
        return curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, pred, curr);
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, pred, curr);
            if (curr->val != key) {
                return false;
            }
            Node* succ = get_next(curr);
            // Logically delete curr
            Node* marked_next = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_next,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                continue; // retry if failed
            }
            // Physically unlink
            if (pred->next.compare_exchange_strong(curr, succ,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                delete curr;
                return true;
            }
            // If physical unlink fails, retry (curr remains logically deleted)
        }
    }
};