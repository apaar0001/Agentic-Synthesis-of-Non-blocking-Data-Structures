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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* cur = head.load(std::memory_order_acquire);
        while (cur != nullptr) {
            Node* next = cur->next.load(std::memory_order_acquire);
            Node* unmarkedNext = get_unmarked_ref(next);
            delete cur;
            cur = unmarkedNext;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* tailPtr = tail.load(std::memory_order_acquire);
            Node* nextPtr = tailPtr->next.load(std::memory_order_acquire);
            if (tailPtr == tail.load(std::memory_order_acquire)) {
                if (nextPtr == nullptr) {
                    if (tailPtr->next.compare_exchange_weak(nextPtr, newNode,
                                                            std::memory_order_release,
                                                            std::memory_order_relaxed)) {
                                                                std::this_thread::sleep_for(std::chrono::seconds(5));
                        tail.compare_exchange_weak(tailPtr, newNode,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_weak(tailPtr, nextPtr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* headPtr = head.load(std::memory_order_acquire);
            Node* tailPtr = tail.load(std::memory_order_acquire);
            Node* nextPtr = headPtr->next.load(std::memory_order_acquire);
            if (headPtr == head.load(std::memory_order_acquire)) {
                if (headPtr == tailPtr) {
                    if (nextPtr == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_weak(tailPtr, nextPtr,
                                               std::memory_order_release,
                                               std::memory_order_relaxed);
                } else {
                    int value = nextPtr->val;
                    Node* markedNext = get_marked_ref(nextPtr);
                    if (headPtr->next.compare_exchange_weak(nextPtr, markedNext,
                                                            std::memory_order_acq_rel,
                                                            std::memory_order_acquire)) {
                                                                std::this_thread::sleep_for(std::chrono::seconds(5));
                        Node* succ = nextPtr->next.load(std::memory_order_acquire);
                        succ = get_unmarked_ref(succ);
                        head.compare_exchange_strong(headPtr, succ,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed);
                        delete nextPtr;
                        return value;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* headPtr = head.load(std::memory_order_acquire);
        Node* nextPtr = headPtr->next.load(std::memory_order_acquire);
        return get_unmarked_ref(nextPtr) == nullptr;
    }
};