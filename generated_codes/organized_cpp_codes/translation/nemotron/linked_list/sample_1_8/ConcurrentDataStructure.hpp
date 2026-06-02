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
        Node(int v, Node* n) : val(v), next(n) {}
    };

    static Node* get_unmarked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    Node* head;
    Node* tail;

    void find(int key, Node*& pred, Node*& curr) {
        retry:
        pred = head;
        curr = get_unmarked(pred->next.load(std::memory_order_acquire));
        while (true) {
            Node* succ_raw = curr->next.load(std::memory_order_acquire);
            bool succ_marked = is_marked(succ_raw);
            Node* succ = get_unmarked(succ_raw);

            Node* pred_next_raw = pred->next.load(std::memory_order_acquire);
            if (is_marked(pred_next_raw) || get_unmarked(pred_next_raw) != curr) {
                goto retry;
            }

            if (!succ_marked && curr->val >= key) {
                return;
            }

            if (succ_marked) {
                Node* succ_next_raw = succ->next.load(std::memory_order_acquire);
                Node* succ_next = get_unmarked(succ_next_raw);
                if (curr->next.compare_exchange_strong(succ_raw, succ_next,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    continue;
                } else {
                    goto retry;
                }
            } else {
                pred = curr;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, nullptr);
        tail = new Node(INT_MAX, nullptr);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = get_unmarked(head->next.load(std::memory_order_acquire));
        while (curr != tail) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            if (is_marked(next_raw)) {
                curr = get_unmarked(next_raw);
                continue;
            }
            if (curr->val >= key) {
                break;
            }
            curr = get_unmarked(next_raw);
        }
        if (curr != tail && curr->val == key) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            return !is_marked(next_raw);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr != tail && curr->val == key) {
                return false;
            }
            Node* node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            Node* succ = get_unmarked(curr->next.load(std::memory_order_acquire));

            Node* pred_next_raw = pred->next.load(std::memory_order_acquire);
            if (is_marked(pred_next_raw) || get_unmarked(pred_next_raw) != curr) {
                continue;
            }

            if (curr == tail || curr->val != key) {
                return false;
            }

            Node* marked_pred_next = get_marked(pred_next_raw);
            if (pred->next.compare_exchange_strong(pred_next_raw, marked_pred_next,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                // Node has been marked
                pred->next.compare_exchange_strong(marked_pred_next, succ,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
                return true;
            }
        }
    }
};