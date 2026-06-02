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
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    Node* head;
    Node* tail;

    void find(int key, Node** pred, Node** curr) {
        *pred = head;
        *curr = (*pred)->next.load(std::memory_order_acquire);
        while (true) {
            while (is_marked_ref(*curr)) {
                Node* succ = get_unmarked_ref((*curr)->next.load(std::memory_order_acquire));
                (*pred)->next.compare_exchange_strong(*curr, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire);
                *curr = (*pred)->next.load(std::memory_order_acquire);
            }
            Node* next = (*curr)->next.load(std::memory_order_acquire);
            while (is_marked_ref(next)) {
                Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
                (*curr)->next.compare_exchange_strong(next, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire);
                next = (*curr)->next.load(std::memory_order_acquire);
            }
            if ((*curr)->val >= key) {
                return;
            }
            *pred = *curr;
            *curr = next;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_release);
        tail->next.store(nullptr, std::memory_order_release);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;
        while (true) {
            curr = pred->next.load(std::memory_order_acquire);
            while (is_marked_ref(curr)) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                pred->next.compare_exchange_strong(curr, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire);
                curr = pred->next.load(std::memory_order_acquire);
            }
            if (curr == tail || curr->val >= key) break;
            pred = curr;
        }
        return (curr != tail && curr->val == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr != tail && curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr == tail || curr->val != key) {
                return false;
            }
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                return false;
            }
            Node* marked_succ = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, marked_succ,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            pred->next.compare_exchange_strong(curr, succ,
                std::memory_order_acq_rel, std::memory_order_acquire);
            return true;
        }
    }
};