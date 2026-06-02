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
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::atomic<Node*> head;

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(INT_MIN);
        head.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        // Leak nodes as permitted
    }

    void add(int val) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            while (curr) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    succ = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                }
                if (pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed)) {
                    if (curr->val >= val) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            if (!curr || curr->val > val) {
                Node* node = new Node(val);
                node->next.store(curr, std::memory_order_relaxed);
                if (pred->next.compare_exchange_strong(curr, node,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed)) {
                    return;
                }
            } else {
                return;
            }
        }
    }

    bool remove(int val) override {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred->next.load(std::memory_order_acquire);
            while (curr) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    succ = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                }
                if (pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed)) {
                    if (curr->val >= val) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            if (!curr || curr->val > val) {
                return false;
            }
            Node* succ = curr->next.load(std::memory_order_acquire);
            if (curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed)) {
                pred->next.compare_exchange_strong(curr, succ,
                                                  std::memory_order_release,
                                                  std::memory_order_relaxed);
                return true;
            }
        }
    }

    bool contains(int val) override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr) {
            if (is_marked_ref(curr)) {
                curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                continue;
            }
            if (curr->val >= val) {
                break;
            }
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
        return (curr && curr->val == val);
    }
};