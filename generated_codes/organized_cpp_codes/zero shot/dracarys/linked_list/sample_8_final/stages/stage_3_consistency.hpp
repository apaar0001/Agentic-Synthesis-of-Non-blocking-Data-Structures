#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        Node* minNode = new Node(INT_MIN);
        Node* maxNode = new Node(INT_MAX);
        minNode->next = maxNode;
        head = minNode;
    }

    ~ConcurrentDataStructure() override {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next);
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* current = head;
        while (true) {
            Node* next = get_unmarked_ref(current->next);
            if (next == nullptr || next->val >= key) {
                return next != nullptr && next->val == key;
            }
            current = next;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* current = head;
            Node* next = get_unmarked_ref(current->next);
            while (next != nullptr && next->val < key) {
                current = next;
                next = get_unmarked_ref(current->next);
            }
            if (next != nullptr && next->val == key) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->next = next;
            if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* current = head;
            Node* next = get_unmarked_ref(current->next);
            while (next != nullptr && next->val < key) {
                current = next;
                next = get_unmarked_ref(current->next);
            }
            if (next == nullptr || next->val != key) {
                return false;
            }
            Node* nextNext = get_unmarked_ref(next->next);
            if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                if (current->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                    delete next;
                    return true;
                }
            }
        }
    }

private:
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

    Node* head;
};