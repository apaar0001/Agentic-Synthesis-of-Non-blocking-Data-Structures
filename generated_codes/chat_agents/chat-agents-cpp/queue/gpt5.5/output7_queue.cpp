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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    void help_remove_marked(Node* pred, Node* curr) {
        if (pred == nullptr || curr == nullptr) {
            return;
        }

        Node* succ = curr->next.load(std::memory_order_acquire);
        Node* unmarked_succ = get_unmarked_ref(succ);

        Node* expected = curr;
        pred->next.compare_exchange_strong(
            expected,
            unmarked_succ,
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
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);

        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* unmarked_last = get_unmarked_ref(last);

            Node* next = unmarked_last->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);

            if (last != tail.load(std::memory_order_acquire)) {
                continue;
            }

            if (is_marked_ref(next)) {
                tail.compare_exchange_strong(
                    last,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
                continue;
            }

            if (unmarked_next == nullptr) {
                Node* expected = nullptr;

                if (unmarked_last->next.compare_exchange_strong(
                        expected,
                        new_node,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    )) {

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
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* unmarked_first = get_unmarked_ref(first);

            Node* last = tail.load(std::memory_order_acquire);

            Node* next = unmarked_first->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);

            if (first != head.load(std::memory_order_acquire)) {
                continue;
            }

            if (unmarked_next == nullptr) {
                return INT_MIN;
            }

            if (is_marked_ref(next)) {
                help_remove_marked(unmarked_first, unmarked_next);
                continue;
            }

            if (unmarked_first == get_unmarked_ref(last)) {
                tail.compare_exchange_strong(
                    last,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                );
                continue;
            }

            Node* succ = unmarked_next->next.load(std::memory_order_acquire);

            if (!is_marked_ref(succ)) {
                Node* marked_succ = get_marked_ref(get_unmarked_ref(succ));

                if (!unmarked_next->next.compare_exchange_strong(
                        succ,
                        marked_succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire
                    )) {
                    continue;
                }
            }

            int value = unmarked_next->val;

            if (head.compare_exchange_strong(
                    first,
                    unmarked_next,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire
                )) {

                delete unmarked_first;
                return value;
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            Node* first = head.load(std::memory_order_acquire);
            Node* unmarked_first = get_unmarked_ref(first);

            Node* next = unmarked_first->next.load(std::memory_order_acquire);
            Node* unmarked_next = get_unmarked_ref(next);

            if (first != head.load(std::memory_order_acquire)) {
                continue;
            }

            if (unmarked_next == nullptr) {
                return true;
            }

            if (is_marked_ref(next)) {
                help_remove_marked(unmarked_first, unmarked_next);
                continue;
            }

            return false;
        }
    }
};
