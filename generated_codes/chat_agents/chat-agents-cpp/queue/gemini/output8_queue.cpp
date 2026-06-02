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
            Node* next_node = unmarked_curr->next.load(std::memory_order_relaxed);
            delete unmarked_curr;
            curr = next_node;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* unmarked_last = get_unmarked_ref(last);
            Node* next = unmarked_last->next.load(std::memory_order_acquire);

            if (last == tail.load(std::memory_order_relaxed)) {
                if (next == nullptr) {
                    if (unmarked_last->next.compare_exchange_weak(next, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        tail.compare_exchange_strong(last, new_node, std::memory_order_acq_rel, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    Node* unmarked_next = get_unmarked_ref(next);
                    tail.compare_exchange_strong(last, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);
            Node* unmarked_first = get_unmarked_ref(first);
            Node* next = unmarked_first->next.load(std::memory_order_acquire);

            if (first == head.load(std::memory_order_relaxed)) {
                if (unmarked_first == get_unmarked_ref(last)) {
                    if (next == nullptr) {
                        return INT_MIN; 
                    }
                    tail.compare_exchange_strong(last, get_unmarked_ref(next), std::memory_order_acq_rel, std::memory_order_relaxed);
                } else {
                    if (next != nullptr) {
                        Node* unmarked_next = get_unmarked_ref(next);
                        int value = unmarked_next->val;
                        
                        Node* expected_next = unmarked_next->next.load(std::memory_order_acquire);
                        if (is_marked_ref(expected_next)) {
                            head.compare_exchange_strong(first, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                            continue;
                        }
                        
                        Node* marked_next = get_marked_ref(expected_next);
                        if (unmarked_next->next.compare_exchange_weak(expected_next, marked_next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            head.compare_exchange_strong(first, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                            return value;
                        }
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);
            Node* unmarked_first = get_unmarked_ref(first);
            Node* next = unmarked_first->next.load(std::memory_order_acquire);

            if (first == head.load(std::memory_order_relaxed)) {
                if (unmarked_first == get_unmarked_ref(last)) {
                    if (next == nullptr) {
                        return true;
                    }
                    tail.compare_exchange_strong(last, get_unmarked_ref(next), std::memory_order_acq_rel, std::memory_order_relaxed);
                } else {
                    if (next != nullptr) {
                        Node* unmarked_next = get_unmarked_ref(next);
                        Node* next_next = unmarked_next->next.load(std::memory_order_acquire);
                        if (is_marked_ref(next_next)) {
                            head.compare_exchange_strong(first, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed);
                            continue;
                        }
                        return false;
                    }
                }
            }
        }
    }
};
