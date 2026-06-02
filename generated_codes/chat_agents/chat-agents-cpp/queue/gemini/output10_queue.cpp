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

    void help_advance_tail(Node* t, Node* next_unmarked) {
        Node* expected_tail = t;
        tail.compare_exchange_weak(expected_tail, next_unmarked, std::memory_order_acq_rel, std::memory_order_relaxed);
    }

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(0);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* next_node = unmarked_curr ? unmarked_curr->next.load(std::memory_order_relaxed) : nullptr;
            delete unmarked_curr;
            curr = next_node;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* unmarked_t = get_unmarked_ref(t);
            Node* next = unmarked_t->next.load(std::memory_order_acquire);

            if (t == tail.load(std::memory_order_acquire)) {
                if (next != nullptr) {
                    help_advance_tail(t, get_unmarked_ref(next));
                    continue;
                }
                Node* expected_next = nullptr;
                if (unmarked_t->next.compare_exchange_strong(expected_next, new_node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    tail.compare_exchange_strong(t, new_node, std::memory_order_acq_rel, std::memory_order_relaxed);
                    return;
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* t = tail.load(std::memory_order_acquire);
            Node* unmarked_h = get_unmarked_ref(h);
            Node* next = unmarked_h->next.load(std::memory_order_acquire);

            if (h == head.load(std::memory_order_acquire)) {
                if (unmarked_h == get_unmarked_ref(t)) {
                    if (next == nullptr) {
                        return INT_MIN; 
                    }
                    help_advance_tail(t, get_unmarked_ref(next));
                } else {
                    if (next == nullptr) {
                        continue;
                    }
                    Node* unmarked_next = get_unmarked_ref(next);
                    
                    if (is_marked_ref(next)) {
                        Node* expected_head = h;
                        if (head.compare_exchange_strong(expected_head, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            // Safely physically unlinked, but we don't reclaim immediately to avoid ABA/use-after-free
                        }
                        continue;
                    }

                    int value = unmarked_next->val;
                    Node* marked_next = get_marked_ref(unmarked_next);
                    
                    if (unmarked_h->next.compare_exchange_strong(next, marked_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        Node* expected_head = h;
                        head.compare_exchange_strong(expected_head, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* t = tail.load(std::memory_order_acquire);
            Node* unmarked_h = get_unmarked_ref(h);
            Node* next = unmarked_h->next.load(std::memory_order_acquire);

            if (h == head.load(std::memory_order_acquire)) {
                if (unmarked_h == get_unmarked_ref(t)) {
                    return next == nullptr;
                }
                if (next == nullptr) {
                    continue;
                }
                if (is_marked_ref(next)) {
                    Node* expected_head = h;
                    head.compare_exchange_strong(expected_head, get_unmarked_ref(next), std::memory_order_acq_rel, std::memory_order_relaxed);
                    continue;
                }
                return false;
            }
        }
    }
};
