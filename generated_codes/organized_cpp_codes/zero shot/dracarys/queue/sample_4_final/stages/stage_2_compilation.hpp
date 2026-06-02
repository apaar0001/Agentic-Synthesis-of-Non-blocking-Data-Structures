#pragma once
#include "../utils/QueueADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public QueueADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    ConcurrentDataStructure() {
        Node* sentinel = new Node();
        sentinel->val = INT_MIN;
        sentinel->next = nullptr;
        head = sentinel;

        Node* tail_sentinel = new Node();
        tail_sentinel->val = INT_MAX;
        tail_sentinel->next = nullptr;
        tail = tail_sentinel;
        head->next = tail_sentinel;
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* next = get_unmarked_ref(head->next);
            delete head;
            head = next;
        }
    }

    void enqueue(int val) {
        Node* new_node = new Node();
        new_node->val = val;
        new_node->next = nullptr;

        while (true) {
            Node* prev_tail = tail;
            Node* prev_next = get_unmarked_ref(prev_tail->next);
            if (prev_tail == tail) {
                if (prev_next == nullptr) {
                    if (prev_tail->next.compare_exchange_strong(prev_next, new_node, std::memory_order_acq_rel)) {
                        prev_tail->next = new_node;
                        tail.compare_exchange_strong(prev_tail, new_node, std::memory_order_acq_rel);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(prev_tail, prev_next, std::memory_order_acq_rel);
                }
            }
        }
    }

    int dequeue() {
        while (true) {
            Node* prev_head = head;
            Node* prev_tail = tail;
            Node* prev_head_next = get_unmarked_ref(prev_head->next);
            if (prev_head == head) {
                if (prev_head == prev_tail) {
                    if (prev_head_next == nullptr) {
                        return -1;
                    }
                    tail.compare_exchange_strong(prev_tail, prev_head_next, std::memory_order_acq_rel);
                } else {
                    int val = prev_head_next->val;
                    if (head.compare_exchange_strong(prev_head, prev_head_next, std::memory_order_acq_rel)) {
                        delete prev_head;
                        return val;
                    }
                }
            }
        }
    }

    bool isEmpty() {
        return head->next == tail;
    }

private:
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
};