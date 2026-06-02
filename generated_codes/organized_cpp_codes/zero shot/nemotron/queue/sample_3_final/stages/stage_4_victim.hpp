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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static Node* get_unmarked(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static bool is_marked(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr != nullptr) {
            Node* next = get_unmarked(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* t_next_raw = t->next.load(std::memory_order_acquire);
            Node* t_next = get_unmarked(t_next_raw);
            if (t_next == nullptr) {
                if (t->next.compare_exchange_weak(t_next_raw, node,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                                                      std::this_thread::sleep_for(std::chrono::seconds(5));
                    tail.compare_exchange_weak(t, node,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
                    return;
                }
            } else {
                tail.compare_exchange_weak(t, t_next,
                                           std::memory_order_release,
                                           std::memory_order_relaxed);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* t = tail.load(std::memory_order_acquire);
            Node* h_next_raw = h->next.load(std::memory_order_acquire);
            Node* h_next = get_unmarked(h_next_raw);
            if (h == head.load(std::memory_order_acquire)) {
                if (h_next == nullptr) {
                    return -1;
                }
                if (h == t) {
                    tail.compare_exchange_weak(t, h_next,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
                } else {
                    int value = h_next->val;
                    Node* marked = get_marked(h_next);
                    if (h->next.compare_exchange_weak(h_next_raw, marked,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_relaxed)) {
                                                          std::this_thread::sleep_for(std::chrono::seconds(5));
                        Node* h_next_next_raw = h_next->next.load(std::memory_order_acquire);
                        Node* h_next_next = get_unmarked(h_next_next_raw);
                        h->next.compare_exchange_weak(marked, h_next_next,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_relaxed);
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* h_next_raw = h->next.load(std::memory_order_acquire);
        Node* h_next = get_unmarked(h_next_raw);
        return h_next == nullptr;
    }
};