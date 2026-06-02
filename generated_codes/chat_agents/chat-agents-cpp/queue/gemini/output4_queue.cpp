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

    void help_advance_tail(Node* t, Node* n_unmarked) {
        Node* expected_tail = t;
        tail.compare_exchange_weak(expected_tail, n_unmarked, std::memory_order_release, std::memory_order_relaxed);
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* next_node = nullptr;
            if (unmarked_curr != nullptr) {
                next_node = unmarked_curr->next.load(std::memory_order_relaxed);
            }
            delete unmarked_curr;
            curr = next_node;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* t_unmarked = get_unmarked_ref(t);
            Node* n = t_unmarked->next.load(std::memory_order_acquire);
            Node* n_unmarked = get_unmarked_ref(n);

            if (t == tail.load(std::memory_order_relaxed)) {
                if (n_unmarked != nullptr) {
                    help_advance_tail(t_unmarked, n_unmarked);
                } else {
                    if (!is_marked_ref(n)) {
                        Node* expected_next = nullptr;
                        if (t_unmarked->next.compare_exchange_strong(expected_next, new_node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            tail.compare_exchange_strong(t_unmarked, new_node, std::memory_order_release, std::memory_order_relaxed);
                            return;
                        }
                    }
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* h_unmarked = get_unmarked_ref(h);
            Node* t = tail.load(std::memory_order_acquire);
            Node* t_unmarked = get_unmarked_ref(t);
            Node* n = h_unmarked->next.load(std::memory_order_acquire);
            Node* n_unmarked = get_unmarked_ref(n);

            if (h == head.load(std::memory_order_relaxed)) {
                if (h_unmarked == t_unmarked) {
                    if (n_unmarked == nullptr) {
                        return INT_MIN; 
                    }
                    help_advance_tail(t_unmarked, n_unmarked);
                } else {
                    if (n_unmarked != nullptr) {
                        if (!is_marked_ref(n)) {
                            Node* marked_n = get_marked_ref(n_unmarked);
                            if (h_unmarked->next.compare_exchange_strong(n_unmarked, marked_n, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                                int value = n_unmarked->val;
                                if (head.compare_exchange_strong(h_unmarked, n_unmarked, std::memory_order_release, std::memory_order_relaxed)) {
                                    // Intentionally leak h_unmarked due to lack of hazard pointers / safe memory reclamation
                                }
                                return value;
                            }
                        } else {
                            head.compare_exchange_strong(h_unmarked, n_unmarked, std::memory_order_release, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* h = head.load(std::memory_order_acquire);
            Node* h_unmarked = get_unmarked_ref(h);
            Node* t = tail.load(std::memory_order_acquire);
            Node* t_unmarked = get_unmarked_ref(t);
            Node* n = h_unmarked->next.load(std::memory_order_acquire);
            Node* n_unmarked = get_unmarked_ref(n);

            if (h == head.load(std::memory_order_relaxed)) {
                if (h_unmarked == t_unmarked) {
                    if (n_unmarked == nullptr) {
                        return true;
                    }
                    help_advance_tail(t_unmarked, n_unmarked);
                } else {
                    if (n_unmarked != nullptr) {
                        if (!is_marked_ref(n)) {
                            return false;
                        } else {
                            head.compare_exchange_strong(h_unmarked, n_unmarked, std::memory_order_release, std::memory_order_relaxed);
                        }
                    }
                }
            }
        }
    }
};
