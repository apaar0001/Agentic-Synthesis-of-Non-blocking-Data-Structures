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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<std::uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(p) | 1L);
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* head_sentinel = new Node{INT_MIN};
        Node* tail_sentinel = new Node{INT_MAX};
        head_sentinel->next.store(tail_sentinel, std::memory_order_relaxed);
        tail_sentinel->next.store(nullptr, std::memory_order_relaxed);
        head.store(head_sentinel, std::memory_order_relaxed);
        tail.store(tail_sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = head.load(std::memory_order_acquire);
        while (cur != nullptr) {
            Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = next;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node{val};
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            while (true) {
                Node* next = pred->next.load(std::memory_order_acquire);
                Node* unmarked_next = get_unmarked_ref(next);
                Node* tail_ptr = tail.load(std::memory_order_acquire);
                if (unmarked_next == tail_ptr) {
                    break;
                }
                pred = unmarked_next;
            }
            Node* expected = pred->next.load(std::memory_order_acquire);
            if (!is_marked_ref(expected)) {
                Node* desired = new_node;
                if (pred->next.compare_exchange_weak(expected, desired,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                                                         std::this_thread::sleep_for(std::chrono::seconds(5));
                    return;
                }
            }
            // else retry
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
            Node* unmarked_first = get_unmarked_ref(first);
            Node* tail_ptr = tail.load(std::memory_order_acquire);
            if (unmarked_first == tail_ptr) {
                return -1;
            }
            Node* next = first->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(next);
            if (!first->next.compare_exchange_weak(next, marked_next,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                continue;
            }
            Node* succ = get_unmarked_ref(first->next.load(std::memory_order_acquire));
            Node* pred = head.load(std::memory_order_acquire);
            Node* expected = first;
            if (pred->next.compare_exchange_weak(expected, succ,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                                                     std::this_thread::sleep_for(std::chrono::seconds(5));
                int v = first->val;
                delete first;
                return v;
            }
            // else retry
        }
    }

    bool isEmpty() override {
        Node* first = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        return get_unmarked_ref(first) == tail.load(std::memory_order_acquire);
    }
};