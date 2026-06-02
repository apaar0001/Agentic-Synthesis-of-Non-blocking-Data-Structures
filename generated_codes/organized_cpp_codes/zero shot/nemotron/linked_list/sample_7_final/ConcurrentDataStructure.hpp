#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    void find(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head;
            curr = pred->next.load(std::memory_order_acquire);
            while (true) {
                Node* uncurr = get_unmarked_ref(curr);
                Node* next = uncurr->next.load(std::memory_order_acquire);
                if (is_marked_ref(curr)) {
                    // help remove marked node
                    pred->next.compare_exchange_strong(curr, get_unmarked_ref(next),
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_relaxed);
                    curr = pred->next.load(std::memory_order_acquire);
                    continue;
                }
                if (uncurr->val >= key) {
                    return;
                }
                pred = uncurr;
                curr = next;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (true) {
            Node* uncurr = get_unmarked_ref(curr);
            if (uncurr->val >= key) break;
            curr = uncurr->next.load(std::memory_order_acquire);
        }
        return (get_unmarked_ref(curr)->val == key && !is_marked_ref(curr));
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, pred, curr);
            Node* uncurr = get_unmarked_ref(curr);
            if (uncurr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                return true;
            }
            // else retry
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, pred, curr);
            Node* uncurr = get_unmarked_ref(curr);
            if (uncurr->val != key) {
                return false;
            }
            Node* next = uncurr->next.load(std::memory_order_acquire);
            // mark the node's next pointer
            while (!is_marked_ref(next)) {
                if (uncurr->next.compare_exchange_weak(next, get_marked_ref(next),
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_relaxed)) {
                    break;
                }
                next = uncurr->next.load(std::memory_order_acquire);
            }
            // physically unlink
            Node* succ = get_unmarked_ref(next);
            if (pred->next.compare_exchange_strong(curr, succ,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed)) {
                delete uncurr;
                return true;
            }
            // else retry
        }
    }
};