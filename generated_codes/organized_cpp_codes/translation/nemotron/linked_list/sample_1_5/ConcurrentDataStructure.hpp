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

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(0x1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(0x1));
    }
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(0x1)) != 0;
    }

    Node* head;
    Node* tail;

    void find(int key, Node*& pred, Node*& curr) {
        while (true) {
            pred = head;
            curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* marked_next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                    if (!pred->next.compare_exchange_strong(curr, marked_next,
                                                           std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    curr = marked_next;
                    next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                }
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    break;
                }
                if (curr->val >= key) {
                    return;
                }
                pred = curr;
                curr = next;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
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
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, pred, curr);
        return (curr != tail && curr->val == key && !is_marked_ref(pred->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr != tail && curr->val == key) {
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
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr == tail || curr->val != key) {
                return false;
            }
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (pred->next.compare_exchange_strong(curr, get_marked_ref(next),
                                                  std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};