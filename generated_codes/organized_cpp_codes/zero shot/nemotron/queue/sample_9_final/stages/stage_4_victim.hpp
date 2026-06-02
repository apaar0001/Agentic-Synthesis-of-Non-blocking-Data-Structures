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
        Node() : val(0), next(nullptr) {}
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<std::uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<std::uintptr_t>(p) | 1ULL);
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node();
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = get_unmarked_ref(head.load(std::memory_order_relaxed));
        while (cur != nullptr) {
            Node* next = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = next;
        }
    }

    void enqueue(int val) override {
        Node* node = new Node(val);
        while (true) {
            Node* t = tail.load(std::memory_order_acquire);
            Node* t_unmarked = get_unmarked_ref(t);
            Node* tNext = get_unmarked_ref(t_unmarked->next.load(std::memory_order_acquire));
            if (tNext == nullptr) {
                Node* expected = nullptr;
                if (t_unmarked->next.compare_exchange_weak(expected, node,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed)) {
                                                              std::this_thread::sleep_for(std::chrono::seconds(5));
                    tail.compare_exchange_weak(t, node,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
                    return;
                }
            } else {
                tail.compare_exchange_weak(t, tNext,
                                           std::memory_order_release,
                                           std::memory_order_relaxed);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* oldHead = head.load(std::memory_order_acquire);
            Node* oldHeadUnmarked = get_unmarked_ref(oldHead);
            Node* next = get_unmarked_ref(oldHeadUnmarked->next.load(std::memory_order_acquire));
            if (next == nullptr) {
                return -1;
            }
            Node* markedNext = get_marked_ref(next);
            Node* expected = next;
            if (oldHeadUnmarked->next.compare_exchange_weak(expected, markedNext,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_relaxed)) {
                                                                std::this_thread::sleep_for(std::chrono::seconds(5));
                Node* expectedHead = oldHead;
                if (head.compare_exchange_weak(expectedHead, next,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) {
                                                   std::this_thread::sleep_for(std::chrono::seconds(5));
                    int val = next->val;
                    delete oldHead;
                    return val;
                }
            }
        }
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* hUnmarked = get_unmarked_ref(h);
        Node* next = get_unmarked_ref(hUnmarked->next.load(std::memory_order_acquire));
        return next == nullptr;
    }
};