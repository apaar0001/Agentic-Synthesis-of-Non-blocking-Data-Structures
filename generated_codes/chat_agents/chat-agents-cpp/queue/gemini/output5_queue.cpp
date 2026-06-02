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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* get_clean_next(Node* curr, Node*& next_raw) {
        while (curr) {
            next_raw = curr->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next_raw);
            if (!is_marked_ref(next_raw)) {
                return unmarked_next;
            }
            Node* expected = next_raw;
            Node* succ_raw = unmarked_next ? unmarked_next->next.load(std::memory_order_acquire) : nullptr;
            Node* unmarked_succ = get_unmarked_ref(succ_raw);
            
            Node* pred_next_raw = curr->next.load(std::memory_order_acquire);
            if (pred_next_raw == expected) {
                if (curr->next.compare_exchange_weak(expected, unmarked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    delete unmarked_next;
                }
            }
        }
        return nullptr;
    }

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(0);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = get_unmarked_ref(head.load(std::memory_order_relaxed));
        while (curr) {
            Node* next_raw = curr->next.load(std::memory_order_relaxed);
            Node* next_node = get_unmarked_ref(next_raw);
            delete curr;
            curr = next_node;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* t = get_unmarked_ref(tail.load(std::memory_order_acquire));
            Node* t_next_raw;
            Node* t_next = get_clean_next(t, t_next_raw);

            if (t == get_unmarked_ref(tail.load(std::memory_order_acquire))) {
                if (t_next == nullptr) {
                    Node* expected = nullptr;
                    if (t->next.compare_exchange_weak(expected, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(t, new_node, std::memory_order_acq_rel, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(t, t_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = get_unmarked_ref(head.load(std::memory_order_acquire));
            Node* t = get_unmarked_ref(tail.load(std::memory_order_acquire));
            Node* h_next_raw;
            Node* h_next = get_clean_next(h, h_next_raw);

            if (h == get_unmarked_ref(head.load(std::memory_order_acquire))) {
                if (h == t) {
                    if (h_next == nullptr) {
                        return INT_MIN;
                    }
                    tail.compare_exchange_strong(t, h_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                } else {
                    if (h_next != nullptr) {
                        int val = h_next->val;
                        Node* expected = h_next_raw;
                        Node* marked_next = get_marked_ref(get_unmarked_ref(h_next_raw));
                        if (h_next->next.compare_exchange_weak(expected, marked_next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            if (head.compare_exchange_strong(h, h_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                delete h;
                            }
                            return val;
                        }
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* h = get_unmarked_ref(head.load(std::memory_order_acquire));
            Node* t = get_unmarked_ref(tail.load(std::memory_order_acquire));
            Node* h_next_raw;
            Node* h_next = get_clean_next(h, h_next_raw);

            if (h == get_unmarked_ref(head.load(std::memory_order_acquire))) {
                if (h == t) {
                    return h_next == nullptr;
                }
                return false;
            }
        }
    }
};
