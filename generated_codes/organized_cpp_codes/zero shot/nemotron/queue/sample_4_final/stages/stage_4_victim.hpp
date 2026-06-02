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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* dummyHead = new Node(INT_MIN);
        Node* dummyTail = new Node(INT_MAX);
        dummyHead->next.store(dummyTail, std::memory_order_relaxed);
        head.store(dummyHead, std::memory_order_relaxed);
        tail.store(dummyTail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = get_unmarked_ref(head.load(std::memory_order_acquire));
        while (cur) {
            Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* t = get_unmarked_ref(tail.load(std::memory_order_acquire));
            Node* tNext = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (t != get_unmarked_ref(tail.load(std::memory_order_acquire))) {
                continue;
            }
            if (tNext == nullptr) {
                Node* expected = nullptr;
                if (t->next.compare_exchange_strong(expected, newNode,
                                                    std::memory_order_release,
                                                    std::memory_order_relaxed)) {
                                                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    tail.compare_exchange_strong(t, newNode,
                                                std::memory_order_release,
                                                std::memory_order_relaxed);
                    return;
                }
            } else {
                tail.compare_exchange_strong(t, tNext,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* h = get_unmarked_ref(head.load(std::memory_order_acquire));
            Node* t = get_unmarked_ref(tail.load(std::memory_order_acquire));
            Node* hNext = get_unmarked_ref(h->next.load(std::memory_order_acquire));
            if (h != get_unmarked_ref(head.load(std::memory_order_acquire))) {
                continue;
            }
            if (h == t) {
                if (hNext == nullptr) {
                    return -1;
                }
                tail.compare_exchange_strong(t, hNext,
                                            std::memory_order_release,
                                            std::memory_order_relaxed);
            } else {
                int val = hNext->val;
                if (head.compare_exchange_strong(h, hNext,
                                                std::memory_order_release,
                                                std::memory_order_relaxed)) {
                                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    Node* toDelete = h;
                    Node* expected = get_unmarked_ref(toDelete->next.load(std::memory_order_acquire));
                    Node* desired = get_marked_ref(expected);
                    while (!toDelete->next.compare_exchange_strong(expected, desired,
                                                                   std::memory_order_release,
                                                                   std::memory_order_relaxed)) {
                        expected = get_unmarked_ref(expected);
                        desired = get_marked_ref(expected);
                    }
                    delete toDelete;
                    return val;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = get_unmarked_ref(head.load(std::memory_order_acquire));
        Node* hNext = get_unmarked_ref(h->next.load(std::memory_order_acquire));
        return hNext == nullptr;
    }
};