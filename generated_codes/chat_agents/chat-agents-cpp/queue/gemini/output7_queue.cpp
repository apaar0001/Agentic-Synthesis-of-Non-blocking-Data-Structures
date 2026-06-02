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

        Node(int v) : val(v), next(nullptr) {}
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

    void help_advance_head() {
        while (true) {
            Node* curr_head = head.load(std::memory_order_acquire);
            Node* next_node = curr_head->next.load(std::memory_order_acquire);
            if (!next_node) {
                break;
            }
            if (is_marked_ref(next_node)) {
                Node* unmarked_next = get_unmarked_ref(next_node);
                if (head.compare_exchange_weak(curr_head, unmarked_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    // Safe to delete logically and physically unlinked sentinel node
                    delete curr_head;
                }
            } else {
                break;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        Node* sentinel = new Node(0);
        head.store(sentinel, std::memory_order_relaxed);
        tail.store(sentinel, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head.load(std::memory_order_relaxed);
        while (curr != nullptr) {
            Node* next_node = curr->next.load(std::memory_order_relaxed);
            delete curr;
            curr = get_unmarked_ref(next_node);
        }
    }

    void enqueue(int val) override {
        Node* new_node = new Node(val);
        while (true) {
            Node* last = tail.load(std::memory_order_acquire);
            Node* next_node = last->next.load(std::memory_order_acquire);
            
            if (last == tail.load(std::memory_order_acquire)) {
                if (next_node == nullptr) {
                    if (last->next.compare_exchange_weak(next_node, new_node, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        tail.compare_exchange_strong(last, new_node, std::memory_order_acq_rel, std::memory_order_relaxed);
                        return;
                    }
                } else {
                    tail.compare_exchange_strong(last, get_unmarked_ref(next_node), std::memory_order_acq_rel, std::memory_order_relaxed);
                }
            }
        }
    }

    int dequeue() override {
        while (true) {
            help_advance_head();
            Node* curr_head = head.load(std::memory_order_acquire);
            Node* first = curr_head->next.load(std::memory_order_acquire);
            Node* last = tail.load(std::memory_order_acquire);

            if (curr_head != head.load(std::memory_order_acquire)) {
                continue;
            }

            if (first == nullptr) {
                return INT_MIN; 
            }

            if (is_marked_ref(first)) {
                continue;
            }

            if (curr_head == last) {
                tail.compare_exchange_strong(last, get_unmarked_ref(first), std::memory_order_acq_rel, std::memory_order_relaxed);
                continue;
            }

            int result = first->val;
            Node* expected_next = first->next.load(std::memory_order_acquire);
            if (is_marked_ref(expected_next)) {
                continue;
            }

            Node* marked_next = get_marked_ref(expected_next);
            if (first->next.compare_exchange_weak(expected_next, marked_next, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                if (head.compare_exchange_strong(curr_head, first, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    delete curr_head;
                }
                return result;
            }
        }
    }

    bool isEmpty() override {
        while (true) {
            help_advance_head();
            Node* curr_head = head.load(std::memory_order_acquire);
            Node* next_node = curr_head->next.load(std::memory_order_acquire);
            if (curr_head == head.load(std::memory_order_acquire)) {
                return next_node == nullptr;
            }
        }
    }
};
