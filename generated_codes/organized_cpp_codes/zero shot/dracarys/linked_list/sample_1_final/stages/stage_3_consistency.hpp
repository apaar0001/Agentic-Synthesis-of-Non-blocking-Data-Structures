#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    ConcurrentDataStructure() {
        Node* min = new Node(INT_MIN);
        Node* max = new Node(INT_MAX);
        min->next = max;
        head = min;
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) {
        Node* curr = head;
        while (true) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (next == nullptr || next->val >= key) {
                return next != nullptr && next->val == key;
            }
            curr = next;
        }
    }

    bool add(int key) {
        while (true) {
            Node* curr = head;
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (next != nullptr && next->val < key) {
                curr = next;
                next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (next != nullptr && next->val == key) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->next = next;
            if (curr->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* curr = head;
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (next != nullptr && next->val < key) {
                curr = next;
                next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (next == nullptr || next->val != key) {
                return false;
            }
            Node* nextNext = get_unmarked_ref(next->next.load(std::memory_order_acquire));
            if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                if (curr->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                    delete next;
                    return true;
                }
            }
        }
    }

private:
    Node* head;
};