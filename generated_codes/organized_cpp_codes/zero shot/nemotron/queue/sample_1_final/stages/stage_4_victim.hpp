#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public QueueADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    Node* tail_sentinel_; // INT_MAX sentinel, allocated but not linked

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        tail_sentinel_ = new Node(INT_MAX);
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
        dummy->next.store(nullptr, std::memory_order_relaxed);
        tail_sentinel_->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = get_unmarked_ref(head_.load(std::memory_order_acquire));
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = nxt;
        }
        delete tail_sentinel_;
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        new_node->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* t = tail_.load(std::memory_order_acquire);
            Node* next = t->next.load(std::memory_order_acquire);
            if (t == tail_.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    if (t->next.compare_exchange_weak(next, new_node,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                                                          std::this_thread::sleep_for(std::chrono::seconds(5));
                        tail_.compare_exchange_weak(t, new_node,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(t, next,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head_.load(std::memory_order_acquire);
            Node* t = tail_.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(h->next.load(std::memory_order_acquire));
            if (h == head_.load(std::memory_order_acquire)) {
                if (h == t) {
                    if (next == nullptr) {
                        return -1;
                    }
                    tail_.compare_exchange_weak(t, next,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire);
                } else {
                    int val = next->val;
                    Node* marked_next = get_marked_ref(next);
                    if (h->next.compare_exchange_weak(next, marked_next,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                                                          std::this_thread::sleep_for(std::chrono::seconds(5));
                        Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                        head_.compare_exchange_weak(h, succ,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire);
                        delete next;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = head_.load(std::memory_order_acquire);
        Node* t = tail_.load(std::memory_order_acquire);
        Node* next = get_unmarked_ref(h->next.load(std::memory_order_acquire));
        return (h == t && next == nullptr);
    }
};