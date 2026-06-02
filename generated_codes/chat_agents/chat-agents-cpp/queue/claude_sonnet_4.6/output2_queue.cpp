#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

public:
    ConcurrentDataStructure() {
        Node* s = new Node(0);
        head_.store(s, std::memory_order_relaxed);
        tail_.store(s, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* c = get_unmarked_ref(head_.load(std::memory_order_relaxed));
        while (c) { Node* n = get_unmarked_ref(c->next.load(std::memory_order_relaxed)); delete c; c = n; }
    }

    void enqueue(int val) override {
        Node* nd = new Node(val);
        while (true) {
            Node* tl = tail_.load(std::memory_order_acquire);
            Node* tlRaw = get_unmarked_ref(tl);
            Node* nx = tlRaw->next.load(std::memory_order_acquire);
            Node* nxRaw = get_unmarked_ref(nx);
            if (tl == tail_.load(std::memory_order_acquire)) {
                if (nxRaw == nullptr) {
                    Node* exp = nullptr;
                    if (tlRaw->next.compare_exchange_strong(exp, nd, std::memory_order_acq_rel)) {
                        tail_.compare_exchange_strong(tl, nd, std::memory_order_acq_rel);
                        return;
                    }
                } else {
                    tail_.compare_exchange_strong(tl, nxRaw, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* hd = head_.load(std::memory_order_acquire);
            Node* hdRaw = get_unmarked_ref(hd);
            Node* tl = tail_.load(std::memory_order_acquire);
            Node* nx = hdRaw->next.load(std::memory_order_acquire);
            Node* nxRaw = get_unmarked_ref(nx);
            if (hd == head_.load(std::memory_order_acquire)) {
                if (hdRaw == get_unmarked_ref(tl)) {
                    if (nxRaw == nullptr) return INT_MIN;
                    tail_.compare_exchange_strong(tl, nxRaw, std::memory_order_acq_rel);
                } else {
                    if (is_marked_ref(nx)) { head_.compare_exchange_strong(hd, nxRaw, std::memory_order_acq_rel); continue; }
                    int v = nxRaw->val;
                    Node* marked = get_marked_ref(nxRaw);
                    Node* exp = nx;
                    if (nxRaw->next.compare_exchange_strong(exp, get_marked_ref(exp), std::memory_order_acq_rel)) {
                        head_.compare_exchange_strong(hd, nxRaw, std::memory_order_acq_rel);
                        delete hdRaw;
                        return v;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = get_unmarked_ref(head_.load(std::memory_order_acquire));
        Node* t = get_unmarked_ref(tail_.load(std::memory_order_acquire));
        return h == t && get_unmarked_ref(h->next.load(std::memory_order_acquire)) == nullptr;
    }
};
