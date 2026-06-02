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
        explicit Node(int v) : val(v) { next.store(nullptr, std::memory_order_relaxed); }
    };

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    Node* head;
    Node* tail;

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_release);
        tail->next.store(nullptr, std::memory_order_relaxed);
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
        Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                if (!pred->next.compare_exchange_weak(curr, succ,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    break;
                }
                curr = succ;
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (curr->val >= key) break;
            pred = curr;
            curr = succ;
        }
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    if (!pred->next.compare_exchange_weak(curr, succ,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                }
                if (curr->val >= key) break;
                pred = curr;
                curr = succ;
            }
            if (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_weak(curr, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    if (!pred->next.compare_exchange_weak(curr, succ,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        break;
                    }
                    curr = succ;
                    succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                }
                if (curr->val >= key) break;
                pred = curr;
                curr = succ;
            }
            if (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (!curr->next.compare_exchange_weak(succ, get_marked_ref(succ),
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    continue;
                }
                pred->next.compare_exchange_weak(curr, succ,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
                delete curr;
                return true;
            }
            return false;
        }
    }
};