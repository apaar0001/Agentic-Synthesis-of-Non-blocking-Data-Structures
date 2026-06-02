#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
    struct Node {
        int val;
        std::atomic<Node*> next;    // unmarked pointer
        std::atomic<bool>  marked;  // logical deletion flag (split reference)
        Node(int v) : val(v), next(nullptr), marked(false) {}
    };

    Node* head;
    Node* tail;

    bool find(int key, Node*& pred, Node*& curr) {
    retry:
        pred = head;
        curr = pred->next.load(std::memory_order_acquire);
        while (true) {
            if (!curr) return false;
            Node* succ = curr->next.load(std::memory_order_acquire);
            while (curr->marked.load(std::memory_order_acquire)) {
                if (!pred->next.compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed))
                    goto retry;
                delete curr;
                curr = succ;
                if (!curr) return false;
                succ = curr->next.load(std::memory_order_acquire);
            }
            if (curr->val >= key) return curr->val == key;
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
        Node* n = head;
        while (n) {
            Node* nx = n->next.load(std::memory_order_relaxed);
            delete n;
            n = nx;
        }
    }

    bool contains(int key) override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr && curr->val < key)
            curr = curr->next.load(std::memory_order_acquire);
        return curr && curr->val == key && !curr->marked.load(std::memory_order_acquire);
    }

    bool add(int key) override {
        while (true) {
            Node* pred, *curr;
            if (find(key, pred, curr)) return false;
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
            if (!find(key, pred, curr)) return false;
            bool expected = false;
            // Logical deletion via separate atomic bool
            if (!curr->marked.compare_exchange_strong(expected, true,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                continue;
            Node* succ = curr->next.load(std::memory_order_acquire);
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_relaxed);
            return true;
        }
    }
};
