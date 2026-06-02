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
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    void help_remove_marked(Node* pred, Node* curr) {
        if (pred == nullptr || curr == nullptr) {
            return;
        }

        Node* next = curr->next.load(std::memory_order_acquire);
        Node* unmarked_next = get_unmarked_ref(next);

        Node* expected = curr;
        pred->next.compare_exchange_strong(
            expected,
            unmarked_next,
            std::memory_order_acq_rel,
            std::memory_order_acquire
        );
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_release);
        tail.store(dummy, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = get_unmarked_ref(head.load(std::memory_order_acquire));

        while (curr != nullptr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);
            delete curr;
            curr = unmarked_next;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);

        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* last_unmarked = get_unmarked_ref(last);

            Node* next = last_unmarked->next.load(std::memory_order_acquire);
            Node* next_unmarked = get_unmarked_ref(next);

            if (last == tail.load(std::memory_order_acquire)) {
                if (next == nullptr) {
                    Node* expected = nullptr;

                    if (last_unmarked->next.compare_exchange_weak(
                            expected,
                            new_node,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {

                        tail.compare_exchange_strong(
                            last,
                            new_node,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire
                        );

                        return;
                    }
                } else {
                    tail.compare_exchange_strong(
                        last,
                        next_unmarked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* first_unmarked = get_unmarked_ref(first);

            Node* last = tail.load(std::memory_order_acquire);

            Node* next = first_unmarked->next.load(std::memory_order_acquire);
            Node* next_unmarked = get_unmarked_ref(next);

            if (first == head.load(std::memory_order_acquire)) {
                if (next_unmarked == nullptr) {
                    return INT_MIN;
                }

                if (first == last) {
                    tail.compare_exchange_strong(
                        last,
                        next_unmarked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );
                    continue;
                }

                Node* next_next = next_unmarked->next.load(std::memory_order_acquire);

                if (!is_marked_ref(next_next)) {
                    Node* marked_next = get_marked_ref(get_unmarked_ref(next_next));

                    if (!next_unmarked->next.compare_exchange_strong(
                            next_next,
                            marked_next,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        continue;
                    }
                }

                int value = next_unmarked->val;

                if (head.compare_exchange_strong(
                        first,
                        next_unmarked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {

                    help_remove_marked(first_unmarked, next_unmarked);
                    delete first_unmarked;
                    return value;
                }
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* first_unmarked = get_unmarked_ref(first);

            Node* next = first_unmarked->next.load(std::memory_order_acquire);
            Node* next_unmarked = get_unmarked_ref(next);

            if (first == head.load(std::memory_order_acquire)) {
                if (next_unmarked == nullptr) {
                    return true;
                }

                Node* next_next = next_unmarked->next.load(std::memory_order_acquire);

                if (is_marked_ref(next_next)) {
                    Node* expected = first;

                    head.compare_exchange_strong(
                        expected,
                        next_unmarked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    );

                    continue;
                }

                return false;
            }
        }
    }
};
