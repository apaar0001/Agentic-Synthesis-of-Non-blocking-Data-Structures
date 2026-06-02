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

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    Node* const head;
    Node* const tail;

    bool find(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head;
            curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    if (!pred->next.compare_exchange_strong(curr, succ,
                                                          std::memory_order_release,
                                                          std::memory_order_relaxed)) {
                        break;
                    }
                    curr = succ;
                    continue;
                }
                if (curr->val >= key) {
                    return (curr->val == key);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MAX)) {
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
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
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (curr != tail && curr->val < key) {
                pred = curr;
                curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            }
            if (curr != tail && curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_weak(curr, node,
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
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* expected = curr;
            if (pred->next.compare_exchange_strong(expected, get_marked_ref(curr),
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
                // Node has been marked
                pred->next.compare_exchange_strong(get_marked_ref(curr), succ,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed);
                return true;
            }
        }
    }
};