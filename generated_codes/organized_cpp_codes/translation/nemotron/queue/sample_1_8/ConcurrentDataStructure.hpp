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
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        // Nodes are intentionally leaked for lock-freedom safety
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        node->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* t_ptr = tail.load(std::memory_order_acquire);
            Node* t = get_unmarked_ref(t_ptr);
            Node* next_ptr = t->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_ptr)) {
                Node* marked_node = get_unmarked_ref(next_ptr);
                Node* succ_ptr = marked_node->next.load(std::memory_order_acquire);
                if (tail.compare_exchange_weak(t_ptr, get_marked_ref(succ_ptr),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                    continue;
                }
            } else {
                Node* next = get_unmarked_ref(next_ptr);
                if (next == nullptr) {
                    if (t->next.compare_exchange_strong(next_ptr, node,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                        tail.compare_exchange_weak(t_ptr, get_marked_ref(node),
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
                        return;
                    }
                } else {
                    tail.compare_exchange_weak(t_ptr, get_marked_ref(next),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h_ptr = head.load(std::memory_order_acquire);
            Node* h = get_unmarked_ref(h_ptr);
            Node* first_ptr = h->next.load(std::memory_order_acquire);
            if (is_marked_ref(first_ptr)) {
                Node* marked_node = get_unmarked_ref(first_ptr);
                Node* next_ptr = marked_node->next.load(std::memory_order_acquire);
                if (h->next.compare_exchange_strong(first_ptr, next_ptr,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    continue;
                }
            } else {
                Node* first = get_unmarked_ref(first_ptr);
                if (first == nullptr) {
                    return INT_MIN;
                }
                Node* next_ptr = first->next.load(std::memory_order_acquire);
                Node* marked_first_ptr = get_marked_ref(first_ptr);
                if (h->next.compare_exchange_strong(first_ptr, marked_first_ptr,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    // Node has been marked
                    if (head.compare_exchange_strong(h_ptr, marked_first_ptr,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                        return first->val;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h_ptr = head.load(std::memory_order_acquire);
        Node* h = get_unmarked_ref(h_ptr);
        Node* first_ptr = h->next.load(std::memory_order_acquire);
        while (is_marked_ref(first_ptr)) {
            Node* marked_node = get_unmarked_ref(first_ptr);
            Node* next_ptr = marked_node->next.load(std::memory_order_acquire);
            first_ptr = next_ptr;
        }
        Node* first = get_unmarked_ref(first_ptr);
        return (first == nullptr);
    }
};