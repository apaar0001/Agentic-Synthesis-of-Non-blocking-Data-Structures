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

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

public:
    ConcurrentDataStructure() {
        Node* dummy_head = new Node(INT_MIN);
        Node* dummy_tail = new Node(INT_MAX);
        head.store(dummy_head, std::memory_order_relaxed);
        tail.store(dummy_tail, std::memory_order_relaxed);
        dummy_head->next.store(dummy_tail, std::memory_order_relaxed);
        dummy_tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* cur = head.load(std::memory_order_acquire);
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
            delete cur;
            cur = nxt;
        }
    }

    bool add(int val) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr != tail.load(std::memory_order_acquire) && curr->val < val) {
                pred = curr;
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr != tail.load(std::memory_order_acquire) && curr->val == val) {
                return false;
            }
            Node* node = new Node(val);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_release, std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    bool remove(int val) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr != tail.load(std::memory_order_acquire) && curr->val < val) {
                pred = curr;
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr == tail.load(std::memory_order_acquire) || curr->val > val) {
                return false;
            }
            Node* marked = get_marked_ref(curr);
            if (curr->next.compare_exchange_strong(curr->next, marked,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (pred->next.compare_exchange_strong(curr, next,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    return true;
                }
                // Help other threads by snipping the node if we failed
                return true;
            }
        }
    }

    bool contains(int val) override {
        Node* curr = head.load(std::memory_order_acquire);
        while (true) {
            Node* next = curr->next.load(std::memory_order_acquire);
            while (is_marked_ref(next)) {
                Node* unmarked = get_unmarked_ref(next);
                if (!curr->next.compare_exchange_strong(next, unmarked,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    next = curr->next.load(std::memory_order_acquire);
                    continue;
                }
                next = curr->next.load(std::memory_order_acquire);
            }
            if (next == nullptr || get_unmarked_ref(next)->val >= val) {
                break;
            }
            curr = get_unmarked_ref(next);
        }
        Node* next_node = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return (next_node != tail.load(std::memory_order_acquire) && 
                next_node->val == val && 
                !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool isEmpty() override {
        Node* h = head.load(std::memory_order_acquire);
        Node* h_next = get_unmarked_ref(h->next.load(std::memory_order_acquire));
        return (h_next == tail.load(std::memory_order_acquire));
    }
};