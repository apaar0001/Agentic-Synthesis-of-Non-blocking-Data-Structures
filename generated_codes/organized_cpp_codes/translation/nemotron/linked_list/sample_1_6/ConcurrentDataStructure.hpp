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
        return (reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1)) != 0;
    }

    void find(int key, Node** pred, Node** curr) {
        while (true) {
            *pred = head;
            *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
            while (*curr != tail && (*curr)->val < key) {
                Node* next_ptr = (*curr)->next.load(std::memory_order_acquire);
                if (is_marked_ref(next_ptr)) {
                    Node* succ = get_unmarked_ref(next_ptr);
                    if (!(*pred)->next.compare_exchange_strong(*curr, succ,
                                                             std::memory_order_acq_rel,
                                                             std::memory_order_acquire)) {
                        break;
                    }
                    *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
                    continue;
                }
                *pred = *curr;
                *curr = get_unmarked_ref(next_ptr);
            }
            Node* next_ptr = (*pred)->next.load(std::memory_order_acquire);
            if (is_marked_ref(next_ptr)) {
                Node* succ = get_unmarked_ref(next_ptr);
                (*pred)->next.compare_exchange_strong(*curr, succ,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire);
                continue;
            }
            return;
        }
    }

    Node* head;
    Node* tail;

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, &pred, &curr);
            if (curr == tail) return false;
            return (curr->val == key);
        }
    }

    bool add(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, &pred, &curr);
            if (curr != tail && curr->val == key) return false;
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred, *curr;
            find(key, &pred, &curr);
            if (curr == tail || curr->val != key) return false;
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (pred->next.compare_exchange_strong(curr, get_marked_ref(curr),
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};