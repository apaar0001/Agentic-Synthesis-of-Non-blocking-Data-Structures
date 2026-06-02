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
        Node(int v) : val(v) { next.store(nullptr, std::memory_order_relaxed); }
    };

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1ULL) != 0;
    }

    Node* head;
    Node* tail;

    bool find(int key, Node** pred, Node** curr) {
        while (true) {
            *pred = head;
            *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
            Node* succ;
            while (true) {
                succ = get_unmarked_ref((*curr)->next.load(std::memory_order_acquire));
                while (is_marked_ref((*curr)->next.load(std::memory_order_acquire))) {
                    Node* tmp = get_unmarked_ref((*curr)->next.load(std::memory_order_acquire));
                    if (!(*pred)->next.compare_exchange_strong(*curr, tmp,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto outer_retry;
                    }
                }
                if (*curr == tail || (*curr)->val >= key) {
                    return true;
                }
                *pred = *curr;
                *curr = succ;
            }
        outer_retry:
            continue;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = head;
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = nxt;
        }
    }

    bool contains(int key) override {
        Node* pred, *curr;
        find(key, &pred, &curr);
        return (curr != tail && curr->val == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, &pred, &curr);
            if (curr != tail && curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, &pred, &curr);
            if (curr == tail || curr->val != key) {
                return false;
            }
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(next, get_marked_ref(next),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            return true;
        }
    }
};