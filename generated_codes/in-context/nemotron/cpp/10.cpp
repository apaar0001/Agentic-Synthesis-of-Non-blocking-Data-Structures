#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static bool is_marked(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* unmark(Node* p)   { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* mark(Node* p)     { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    Node* head;
    Node* tail;

    struct Window { Node* pred; Node* curr; std::atomic<Node*>* slot; };

    Window find(int key) {
        retry:
        Node* pred = head;
        std::atomic<Node*>* slot = &head->right;
        Node* curr = unmark(slot->load(std::memory_order_acquire));

        while (curr != tail) {
            Node* next_raw = curr->right.load(std::memory_order_acquire);
            if (is_marked(next_raw)) {
                Node* exp = curr;
                if (!slot->compare_exchange_strong(exp, unmark(next_raw), std::memory_order_acq_rel))
                    goto retry;
                curr = unmark(slot->load(std::memory_order_acquire));
                continue;
            }
            if (curr->val >= key) break;
            pred = curr;
            slot = &curr->right;
            curr = unmark(next_raw);
        }
        return { pred, curr, slot };
    }

    void destroy(Node* n) {
        while (n) {
            Node* next = unmark(n->right.load(std::memory_order_relaxed));
            delete n;
            n = next;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->right.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() { destroy(head); }

    bool contains(int key) override {
        Node* cur = unmark(head->right.load(std::memory_order_acquire));
        while (cur != tail && cur->val < key)
            cur = unmark(cur->right.load(std::memory_order_acquire));
        return cur != tail && cur->val == key &&
               !is_marked(cur->right.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        Node* new_node = new Node(key);
        while (true) {
            auto [pred, curr, slot] = find(key);
            if (curr != tail && curr->val == key) { delete new_node; return false; }
            new_node->right.store(curr, std::memory_order_relaxed);
            Node* exp = curr;
            if (slot->compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            auto [pred, curr, slot] = find(key);
            if (curr == tail || curr->val != key) return false;
            Node* next = curr->right.load(std::memory_order_acquire);
            if (is_marked(next)) return false;
            Node* marked = mark(next);
            if (curr->right.compare_exchange_strong(next, marked, std::memory_order_acq_rel)) {
                Node* exp = curr;
                slot->compare_exchange_strong(exp, unmark(next), std::memory_order_acq_rel);
                return true;
            }
        }
    }
};
