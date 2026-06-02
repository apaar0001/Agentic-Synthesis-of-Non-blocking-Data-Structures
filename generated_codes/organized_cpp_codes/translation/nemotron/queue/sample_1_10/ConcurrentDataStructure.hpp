#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
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

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        dummy->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = get_unmarked_ref(head_->load(std::memory_order_acquire)->next.load(std::memory_order_acquire));
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = nxt;
        }
        delete get_unmarked_ref(head_->load(std::memory_order_acquire));
    }

    void enqueue(int val) override {
        while (true) {
            Node* t = tail_.load(std::memory_order_acquire);
            Node* t_next = t->next.load(std::memory_order_acquire);
            if (is_marked_ref(t_next)) {
                Node* marked = t_next;
                Node* unmarked = get_unmarked_ref(marked);
                Node* next_next = unmarked->next.load(std::memory_order_acquire);
                if (t->next.compare_exchange_weak(t_next, next_next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }
            } else {
                if (t_next == nullptr) {
                    Node* node = new Node(val);
                    node->next.store(nullptr, std::memory_order_relaxed);
                    if (t->next.compare_exchange_weak(t_next, node,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail_.compare_exchange_weak(t, node,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(t, get_unmarked_ref(t_next),
                            std::memory_order_acq_rel, std::memory_order_acquire);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head_.load(std::memory_order_acquire);
            Node* h_next = h->next.load(std::memory_order_acquire);
            if (is_marked_ref(h_next)) {
                Node* marked = h_next;
                Node* unmarked = get_unmarked_ref(marked);
                Node* next_next = unmarked->next.load(std::memory_order_acquire);
                if (h->next.compare_exchange_weak(h_next, next_next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }
            } else {
                Node* first = get_unmarked_ref(h_next);
                if (first == nullptr) {
                    return INT_MIN;
                }
                Node* marked_first = get_marked_ref(first);
                if (h->next.compare_exchange_weak(h_next, marked_first,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return first->val;
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* h = head_.load(std::memory_order_acquire);
            Node* h_next = h->next.load(std::memory_order_acquire);
            if (is_marked_ref(h_next)) {
                Node* marked = h_next;
                Node* unmarked = get_unmarked_ref(marked);
                Node* next_next = unmarked->next.load(std::memory_order_acquire);
                if (h->next.compare_exchange_weak(h_next, next_next,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }
            } else {
                Node* first = get_unmarked_ref(h_next);
                return (first == nullptr);
            }
        }
    }
};