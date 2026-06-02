#pragma once
#include "../utils/QueueADT.hpp"
#include "../utils/SetADT.hpp"
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

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;

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
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = head_.load(std::memory_order_acquire);
        while (cur != nullptr) {
            Node* next = cur->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);
            delete cur;
            cur = unmarked_next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = tail->next.load(std::memory_order_acquire);
            if (tail == tail_.load(std::memory_order_acquire)) {
                if (get_unmarked_ref(next) == nullptr) {
                    if (tail->next.compare_exchange_weak(next, newNode,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        tail_.compare_exchange_weak(tail, newNode,
                            std::memory_order_release, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail_.compare_exchange_weak(tail, get_unmarked_ref(next),
                        std::memory_order_release, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* head = head_.load(std::memory_order_acquire);
            Node* tail = tail_.load(std::memory_order_acquire);
            Node* next = head->next.load(std::memory_order_acquire);
            if (head == head_.load(std::memory_order_acquire)) {
                if (head == tail) {
                    if (get_unmarked_ref(next) == nullptr) {
                        return -1;
                    }
                    tail_.compare_exchange_weak(tail, get_unmarked_ref(next),
                        std::memory_order_release, std::memory_order_relaxed);
                } else {
                    int value = next->val;
                    if (head_.compare_exchange_weak(head, next,
                            std::memory_order_release, std::memory_order_relaxed)) {
                        delete head;
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* head = head_.load(std::memory_order_acquire);
        Node* next = head->next.load(std::memory_order_acquire);
        return get_unmarked_ref(next) == nullptr;
    }
};