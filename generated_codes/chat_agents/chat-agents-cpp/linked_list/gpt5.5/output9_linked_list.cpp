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

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool find(int key, Node*& pred, Node*& curr) {
    retry:
        pred = head;
        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));

        while (true) {
            Node* succ = curr->next.load(std::memory_order_acquire);

            while (is_marked_ref(succ)) {
                Node* unmarked_succ = get_unmarked_ref(succ);
                Node* expected = curr;

                if (!pred->next.compare_exchange_strong(
                        expected,
                        unmarked_succ,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    goto retry;
                }

                curr = unmarked_succ;
                succ = curr->next.load(std::memory_order_acquire);
            }

            if (curr->val >= key) {
                return true;
            }

            pred = curr;
            curr = get_unmarked_ref(succ);
        }
    }

public:
    ConcurrentDataStructure() {
        Node* tail = new Node(INT_MAX);
        Node* sentinel = new Node(INT_MIN);
        sentinel->next.store(tail, std::memory_order_release);
        head = sentinel;
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;

        while (curr) {
            Node* next = get_unmarked_ref(
                curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head;

        while (curr->val < key) {
            Node* next = curr->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(next);
        }

        Node* succ = curr->next.load(std::memory_order_acquire);

        return curr->val == key && !is_marked_ref(succ);
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;

            find(key, pred, curr);

            if (curr->val == key) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (!is_marked_ref(succ)) {
                    return false;
                }
                continue;
            }

            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);

            Node* expected = curr;

            if (pred->next.compare_exchange_strong(
                    expected,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }

            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;

            find(key, pred, curr);

            if (curr->val != key) {
                return false;
            }

            Node* succ = curr->next.load(std::memory_order_acquire);

            if (is_marked_ref(succ)) {
                continue;
            }

            Node* marked_succ = get_marked_ref(succ);

            if (!curr->next.compare_exchange_strong(
                    succ,
                    marked_succ,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                continue;
            }

            Node* expected = curr;

            pred->next.compare_exchange_strong(
                expected,
                get_unmarked_ref(succ),
                std::memory_order_acq_rel,
                std::memory_order_acquire);

            return true;
        }
    }
};
