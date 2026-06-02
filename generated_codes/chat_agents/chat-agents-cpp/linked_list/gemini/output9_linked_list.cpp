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

        Node(int key) : val(key), next(nullptr) {}
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

    bool find(int key, Node*& pred, Node*& curr) {
        Node* succ = nullptr;
        bool marked = false;
        bool retry = false;

    retry_loop:
        while (true) {
            pred = head;
            curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));

            while (true) {
                succ = curr->next.load(std::memory_order_acquire);
                marked = is_marked_ref(succ);
                succ = get_unmarked_ref(succ);

                while (marked) {
                    Node* expected = curr;
                    if (!pred->next.compare_exchange_weak(expected, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry_loop;
                    }
                    curr = succ;
                    succ = curr->next.load(std::memory_order_acquire);
                    marked = is_marked_ref(succ);
                    succ = get_unmarked_ref(succ);
                }

                if (curr->val >= key) {
                    return curr->val == key;
                }

                pred = curr;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next_node = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next_node;
        }
    }

    bool add(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;

        while (true) {
            if (find(key, pred, curr)) {
                return false;
            }

            Node* new_node = new Node(key);
            new_node->next.store(curr, std::memory_order_relaxed);

            Node* expected = curr;
            if (pred->next.compare_exchange_weak(expected, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }

            delete new_node;
        }
    }

    bool remove(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;

        while (true) {
            if (!find(key, pred, curr)) {
                return false;
            }

            Node* succ = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                return false;
            }

            Node* unmarked_succ = get_unmarked_ref(succ);
            Node* marked_succ = get_marked_ref(unmarked_succ);

            if (curr->next.compare_exchange_weak(succ, marked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                Node* expected = curr;
                pred->next.compare_exchange_strong(expected, unmarked_succ, std::memory_order_acq_rel, std::memory_order_acquire);
                return true;
            }
        }
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr->val < key) {
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
        Node* succ = curr->next.load(std::memory_order_acquire);
        return curr->val == key && !is_marked_ref(succ);
    }
};
