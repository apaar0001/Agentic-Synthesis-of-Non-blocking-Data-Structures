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

    ConcurrentDataStructure() : head(new Node(INT_MIN)) {
        head->next = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next);
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* prev = head;
        Node* current = prev->next;
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        return current != nullptr && current->val == key && !is_marked_ref(current->next);
    }

    bool add(int key) {
        Node* prev = head;
        Node* current = prev->next;
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current != nullptr && current->val == key) {
            return false;
        }
        Node* newNode = new Node(key);
        newNode->next = current;
        while (!prev->next.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(current->next);
        }
        return true;
    }

    bool remove(int key) {
        Node* prev = head;
        Node* current = prev->next;
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current == nullptr || current->val != key) {
            return false;
        }
        Node* next = get_unmarked_ref(current->next);
        while (!current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
            next = get_unmarked_ref(current->next);
        }
        while (!prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(current->next);
        }
        delete current;
        return true;
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

private:
    Node* head;
};