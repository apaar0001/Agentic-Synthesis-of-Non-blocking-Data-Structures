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

    struct Window {
        Node* pred;
        Node* curr;
    };

    Window find(int key) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;
        bool retry = false;

        while (true) {
            retry = false;
            pred = head;
            curr = pred->next.load(std::memory_order_acquire);

            while (true) {
                Node* succ_raw = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(succ_raw)) {
                    succ = get_unmarked_ref(succ_raw);
                    if (!pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        retry = true;
                        break;
                    }
                    delete curr;
                    curr = succ;
                    succ_raw = curr->next.load(std::memory_order_acquire);
                }

                if (retry) {
                    break;
                }

                if (curr->val >= key) {
                    return Window{pred, curr};
                }

                pred = curr;
                curr = get_unmarked_ref(succ_raw);
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
        while (true) {
            Window w = find(key);
            Node* pred = w.pred;
            Node* curr = w.curr;

            if (curr->val == key) {
                return false;
            } else {
                Node* new_node = new Node(key);
                new_node->next.store(curr, std::memory_order_relaxed);
                if (pred->next.compare_exchange_strong(curr, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return true;
                }
                delete new_node;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Window w = find(key);
            Node* pred = w.pred;
            Node* curr = w.curr;

            if (curr->val != key) {
                return false;
            } else {
                Node* succ = curr->next.load(std::memory_order_acquire);
                if (!is_marked_ref(succ)) {
                    if (curr->next.compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (pred->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            delete curr;
                        }
                        return true;
                    }
                }
            }
        }
    }

    bool contains(int key) override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr != nullptr) {
            Node* next_raw = curr->next.load(std::memory_order_acquire);
            if (!is_marked_ref(next_raw)) {
                if (curr->val >= key) {
                    return curr->val == key;
                }
            }
            curr = get_unmarked_ref(next_raw);
        }
        return false;
    }
};
