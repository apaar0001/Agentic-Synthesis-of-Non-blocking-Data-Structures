#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <utility>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
        Node(int v, Node* n) : val(v), next(n) {}
    };

    Node* head;
    Node* tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    std::pair<Node*, Node*> find(int key) {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                // Help remove marked nodes
                while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    if (!pred->next.compare_exchange_strong(curr, next,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        goto outer_restart;
                    }
                    curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                    succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                }
                // Check for concurrent modifications
                if (!pred->next.compare_exchange_strong(curr, curr,
                                                      std::memory_order_acquire,
                                                      std::memory_order_acquire)) {
                    goto outer_restart;
                }
                if (curr->val >= key) {
                    return {pred, curr};
                }
                pred = curr;
                curr = succ;
            }
        outer_restart:
            continue;
    }
    }

public:
    ConcurrentDataStructure() {
        tail = new Node(INT_MAX, nullptr);
        head = new Node(INT_MIN, tail);
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
        auto [pred, curr] = find(key);
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                                                      std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            // else retry
        }
    }

    bool remove(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            if (curr->val != key) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                                                      std::this_thread::sleep_for(std::chrono::seconds(5));
                // Marked successfully, now try to unlink
                pred->next.compare_exchange_strong(curr, succ,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
                delete curr;
                return true;
            }
            // If marking failed, retry
        }
    }
};