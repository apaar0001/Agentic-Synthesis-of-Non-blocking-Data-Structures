#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <utility>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
        Node(int v, Node* n) : val(v), next(n) {}
    };

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    std::pair<Node*, Node*> find(int key) {
        retry:
        Node* pred = head;
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            // Help remove any marked nodes that are pred->next
            while (is_marked_ref(pred->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    goto retry;
                }
                curr = succ;
            }
            // Now pred->next is unmarked and equals curr
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                // curr became marked, help remove it and restart
                goto retry;
            }
            if (succ == tail || succ->val >= key) {
                return {pred, curr};
            }
            pred = curr;
            curr = succ;
        }
    }

    Node* head;
    Node* tail;

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    virtual ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    virtual bool contains(int key) override {
        auto [pred, curr] = find(key);
        return (curr != tail && curr->val == key);
    }

    virtual bool add(int key) override {
        retry:
        auto [pred, curr] = find(key);
        if (curr != tail && curr->val == key) {
            return false;
        }
        Node* node = new Node(key);
        node->next.store(curr, std::memory_order_relaxed);
        if (!pred->next.compare_exchange_strong(curr, node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            delete node;
            goto retry;
        }
        return true;
    }

    virtual bool remove(int key) override {
        retry:
        auto [pred, curr] = find(key);
        if (curr == tail || curr->val != key) {
            return false;
        }
        Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        if (!curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            goto retry;
        }
        // Node has been marked
        pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
        return true;
    }
};