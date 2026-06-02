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

    void clean_marked_nodes() {
        while (true) {
            Node* curr_head = head.load(std::memory_order_acquire);
            Node* first = get_unmarked_ref(curr_head->next.load(std::memory_order_acquire));
            
            if (!first) return;

            Node* next_ref = first->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_ref)) {
                Node* expected = curr_head;
                if (head.compare_exchange_weak(expected, first, std::memory_order_acq_rel)) {
                    delete curr_head;
                }
            } else {
                return;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(0);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr) {
            Node* next_node = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next_node;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            clean_marked_nodes();
            
            Node* curr_tail = tail.load(std::memory_order_acquire);
            Node* tail_next = curr_tail->next.load(std::memory_order_acquire);

            if (curr_tail != tail.load(std::memory_order_relaxed)) continue;

            if (tail_next != nullptr) {
                Node* unmarked_tail_next = get_unmarked_ref(tail_next);
                tail.compare_exchange_weak(curr_tail, unmarked_tail_next, std::memory_order_acq_rel);
                continue;
            }

            Node* expected = nullptr;
            if (curr_tail->next.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                tail.compare_exchange_strong(curr_tail, new_node, std::memory_order_acq_rel);
                return;
            }
        }
    }

    int dequeue() override {
        while (true) {
            clean_marked_nodes();

            Node* curr_head = head.load(std::memory_order_acquire);
            Node* first = get_unmarked_ref(curr_head->next.load(std::memory_order_acquire));
            Node* curr_tail = tail.load(std::memory_order_acquire);

            if (!first) {
                return INT_MIN; 
            }

            Node* next_ref = first->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_ref)) {
                continue;
            }

            if (curr_head == curr_tail) {
                tail.compare_exchange_strong(curr_tail, first, std::memory_order_acq_rel);
                continue;
            }

            int value = first->val;
            Node* expected = next_ref;
            Node* desired = get_marked_ref(next_ref);

            if (first->next.compare_exchange_strong(expected, desired, std::memory_order_acq_rel)) {
                Node* expected_head = curr_head;
                if (head.compare_exchange_strong(expected_head, first, std::memory_order_acq_rel)) {
                    delete curr_head;
                }
                return value;
            }
        }
    }

    bool isEmpty() override {
        clean_marked_nodes();
        Node* curr_head = head.load(std::memory_order_acquire);
        Node* first = get_unmarked_ref(curr_head->next.load(std::memory_order_acquire));
        return first == nullptr;
    }
};
