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
            while (is_marked(succ_raw)) {
                Node* succ = get_unmarked(succ_raw);
                if (!pred->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire))
                    goto retry;
                curr = succ;
                succ_raw = curr->next.load(std::memory_order_acquire);
            }
            if (curr->val >= key) break;
            pred = curr;
            curr = get_unmarked(succ_raw);
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
            Node* next = get_unmarked(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, pred, curr);
        return (curr != tail && curr->val == key && !is_marked(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr != tail && curr->val == key)
                return false;
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire))
                return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr == tail || curr->val != key)
                return false;
            Node* succ = get_unmarked(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, get_marked(succ),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire))
                continue;
            // Node has been marked
            pred->next.compare_exchange_strong(curr, succ,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
            return true;
        }
    }
};