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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    Node* headSentinel;
    Node* tailSentinel;
    std::atomic<Node*> head; // points to dummy node
    std::atomic<Node*> tail; // points to last real node

    bool is_sentinel(Node* n) {
        return n == headSentinel || n == tailSentinel;
    }

public:
    ConcurrentDataStructure() {
        headSentinel = new Node(INT_MIN);
        tailSentinel = new Node(INT_MAX);
        headSentinel->next.store(tailSentinel, std::memory_order_relaxed);
        head.store(headSentinel, std::memory_order_relaxed);
        tail.store(headSentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = get_unmarked_ref(head.load(std::memory_order_acquire));
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            if (!is_sentinel(cur)) {
                delete cur;
            }
            cur = nxt;
        }
        delete headSentinel;
        delete tailSentinel;
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        newNode->next.store(tailSentinel, std::memory_order_relaxed);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(last->next.load(std::memory_order_acquire));
            if (next == tailSentinel) {
                if (last->next.compare_exchange_weak(next, newNode,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                                                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    tail.compare_exchange_weak(last, newNode,
                                              std::memory_order_acq_rel,
                                              std::memory_order_acquire);
                    return;
                }
            } else {
                tail.compare_exchange_weak(last, next,
                                          std::memory_order_acq_rel,
                                          std::memory_order_acquire);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(first->next.load(std::memory_order_acquire));
            if (get_unmarked_ref(next) == tailSentinel) {
                return -1;
            }
            int value = next->val;
            Node* markedNext = get_marked_ref(next);
            // Try to mark first->next as logically removed
            if (first->next.compare_exchange_strong(next, markedNext,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                                                       std::this_thread::sleep_for(std::chrono::seconds(5));
                // Successfully marked, now swing head to skip the dummy
                if (head.compare_exchange_strong(first, next,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
                                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (!is_sentinel(first)) {
                        delete first;
                    }
                    return value;
                }
                // If head CAS fails, another thread updated head; retry
            } else {
                // Maybe already marked, help advance head
                Node* curNext = first->next.load(std::memory_order_acquire);
                Node* curNextUnmarked = get_unmarked_ref(curNext);
                if (head.compare_exchange_strong(first, curNextUnmarked,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
                                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    if (!is_sentinel(first)) {
                        delete first;
                    }
                    return value;
                }
                // else retry
            }
        }
    }

    bool isEmpty() override {
        Node* first = head.load(std::memory_order_acquire);
        Node* next = get_unmarked_ref(first->next.load(std::memory_order_acquire));
        return next == tailSentinel;
    }
};