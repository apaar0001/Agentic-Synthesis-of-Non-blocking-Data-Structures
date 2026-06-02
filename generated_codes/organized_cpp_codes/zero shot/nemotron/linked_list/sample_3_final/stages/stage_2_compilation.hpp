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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    Node* head;

    std::pair<Node*, Node*> find(int key) {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                bool marked = is_marked_ref(curr->next.load(std::memory_order_acquire));
                while (marked) {
                    Node* succNext = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                    if (pred->next.compare_exchange_strong(curr, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                    succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    marked = is_marked_ref(curr->next.load(std::memory_order_acquire));
                }
                if (curr->val >= key) {
                    return {pred, curr};
                }
                pred = curr;
                curr = succ;
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
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr != nullptr && curr->val != INT_MAX) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
    }

    bool contains(int key) override {
        auto [pred, curr] = find(key);
        Node* next = curr->next.load(std::memory_order_acquire);
        return (curr->val == key) && !is_marked_ref(next);
    }

    bool add(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            Node* next = curr->next.load(std::memory_order_acquire);
            if (curr->val == key && !is_marked_ref(next)) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            auto [pred, curr] = find(key);
            Node* next = curr->next.load(std::memory_order_acquire);
            if (curr->val != key || is_marked_ref(next)) {
                return false;
            }
            Node* succ = get_unmarked_ref(next);
            if (!curr->next.compare_exchange_strong(next, get_marked_ref(succ),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            pred->next.compare_exchange_strong(curr, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            delete curr;
            return true;
        }
    }
};