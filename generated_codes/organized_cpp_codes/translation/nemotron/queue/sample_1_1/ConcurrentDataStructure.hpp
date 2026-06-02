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

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(
            reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(
            reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::atomic<Node*> head;
    std::atomic<Node*> tail;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        head.store(dummy, std::memory_order_relaxed);
        tail.store(dummy, std::memory_order_relaxed);
    }

    void add(int val) override {
        Node* node = new Node(val);
        node->next.store(nullptr, std::memory_order_relaxed);
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            Node* last = pred;
            bool duplicate = false;
            while (curr != nullptr) {
                if (is_marked_ref(curr)) {
                    Node* next = curr->next.load(std::memory_order_acquire);
                    pred->next.compare_exchange_strong(curr, next,
                            std::memory_order_release, std::memory_order_relaxed);
                    curr = next;
                    continue;
                }
                if (curr->val == val) {
                    duplicate = true;
                    break;
                }
                last = curr;
                pred = curr;
                curr = curr->next.load(std::memory_order_acquire);
            }
            if (duplicate) {
                delete node;
                return;
            }
            Node* last_next = last->next.load(std::memory_order_acquire);
            if (last_next == nullptr) {
                if (last->next.compare_exchange_weak(last_next, node,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    tail.compare_exchange_weak(last, node,
                            std::memory_order_release, std::memory_order_relaxed);
                    return;
                }
            } else {
                continue;
            }
        }
    }

    bool remove(int val) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            bool found = false;
            while (curr != nullptr) {
                if (is_marked_ref(curr)) {
                    Node* next = curr->next.load(std::memory_order_acquire);
                    pred->next.compare_exchange_strong(curr, next,
                            std::memory_order_release, std::memory_order_relaxed);
                    curr = next;
                    continue;
                }
                if (curr->val == val) {
                    found = true;
                    break;
                }
                pred = curr;
                curr = curr->next.load(std::memory_order_acquire);
            }
            if (!found) {
                return false;
            }
            Node* next = curr->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(next);
            if (curr->next.compare_exchange_weak(next, marked_next,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }

    bool contains(int val) override {
        Node* curr = head.load(std::memory_order_acquire)->next.load(std::memory_order_acquire);
        while (curr != nullptr) {
            if (is_marked_ref(curr)) {
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                continue;
            }
            if (curr->val == val) {
                return true;
            }
            curr = curr->next.load(std::memory_order_acquire);
        }
        return false;
    }

    bool isEmpty() override {
        Node* curr = head.load(std::memory_order_acquire);
        while (true) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                Node* unmarked_next = get_unmarked_ref(next);
                Node* next_next = unmarked_next->next.load(std::memory_order_acquire);
                if (curr->next.compare_exchange_strong(next, get_marked_ref(next_next),
                        std::memory_order_release, std::memory_order_relaxed)) {
                    continue;
                } else {
                    continue;
                }
            }
            if (get_unmarked_ref(next) == nullptr) {
                return true;
            }
            return false;
        }
    }
};