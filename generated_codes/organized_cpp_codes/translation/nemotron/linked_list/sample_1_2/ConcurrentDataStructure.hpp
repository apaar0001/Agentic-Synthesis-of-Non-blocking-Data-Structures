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

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    Node* head;
    Node* tail;

    bool find(int key, Node*& pred, Node*& curr) {
        retry:
        pred = head;
        curr = pred->next.load(std::memory_order_acquire);
        while (true) {
            if (is_marked_ref(curr)) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (!pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    goto retry;
                }
                curr = succ;
                continue;
            }
            if (curr == tail || curr->val >= key) {
                break;
            }
            pred = curr;
            curr = curr->next.load(std::memory_order_acquire);
        }
        return (curr != tail && curr->val == key);
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, nullptr);
        tail = new Node(INT_MAX, nullptr);
        head->next.store(tail, std::memory_order_relaxed);
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(next_raw);
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;
        return find(key, pred, curr);
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (find(key, pred, curr)) {
                return false;
            }
            Node* node = new Node(key, curr);
            if (pred->next.compare_exchange_strong(curr, node,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, pred, curr)) {
                return false;
            }
            if (pred->next.compare_exchange_strong(curr, get_marked_ref(curr),
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                // Node has been marked
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                pred->next.compare_exchange_strong(get_marked_ref(curr), succ,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                return true;
            }
        }
    }
};