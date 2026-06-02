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

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(
            reinterpret_cast<uintptr_t>(p) & ~1L
        );
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(
            reinterpret_cast<uintptr_t>(p) | 1L
        );
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_release);
        tail.store(dummy, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = get_unmarked_ref(
            head.load(std::memory_order_acquire)
        );

        while (curr) {
            Node* next = get_unmarked_ref(
                curr->next.load(std::memory_order_acquire)
            );
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);

        while (true) {
            Node* last = get_unmarked_ref(
                tail.load(std::memory_order_acquire)
            );

            Node* next = last->next.load(std::memory_order_acquire);

            if (is_marked_ref(next)) {
                Node* unmarkedNext = get_unmarked_ref(next);
                tail.compare_exchange_weak(
                    last,
                    unmarkedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
                continue;
            }

            if (last == get_unmarked_ref(
                    tail.load(std::memory_order_acquire))) {

                if (next == nullptr) {
                    Node* expected = nullptr;

                    if (last->next.compare_exchange_weak(
                            expected,
                            newNode,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {

                        tail.compare_exchange_weak(
                            last,
                            newNode,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire
                        );

                        return;
                    }
                } else {
                    Node* unmarkedNext = get_unmarked_ref(next);

                    tail.compare_exchange_weak(
                        last,
                        unmarkedNext,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = get_unmarked_ref(
                head.load(std::memory_order_acquire)
            );

            Node* last = get_unmarked_ref(
                tail.load(std::memory_order_acquire)
            );

            Node* next = first->next.load(std::memory_order_acquire);

            if (next == nullptr) {
                return INT_MIN;
            }

            Node* unmarkedNext = get_unmarked_ref(next);

            if (first != get_unmarked_ref(
                    head.load(std::memory_order_acquire))) {
                continue;
            }

            if (is_marked_ref(next)) {
                head.compare_exchange_weak(
                    first,
                    unmarkedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
                continue;
            }

            if (first == last) {
                tail.compare_exchange_weak(
                    last,
                    unmarkedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
                continue;
            }

            Node* expectedNext = next;
            Node* markedNext = get_marked_ref(unmarkedNext);

            if (!first->next.compare_exchange_strong(
                    expectedNext,
                    markedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            int value = unmarkedNext->val;

            if (head.compare_exchange_strong(
                    first,
                    unmarkedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                delete first;
            }

            return value;
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* first = get_unmarked_ref(
                head.load(std::memory_order_acquire)
            );

            Node* next = first->next.load(std::memory_order_acquire);

            if (first != get_unmarked_ref(
                    head.load(std::memory_order_acquire))) {
                continue;
            }

            if (next == nullptr) {
                return true;
            }

            if (is_marked_ref(next)) {
                Node* unmarkedNext = get_unmarked_ref(next);

                head.compare_exchange_weak(
                    first,
                    unmarkedNext,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );

                continue;
            }

            return false;
        }
    } 
};
