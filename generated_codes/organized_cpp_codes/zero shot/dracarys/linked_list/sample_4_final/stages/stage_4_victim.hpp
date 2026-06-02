#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head_(new Node(INT_MIN)) {
        head_->next = get_marked_ref(new Node(INT_MAX));
    }

    ~ConcurrentDataStructure() {
        Node* current = head_;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next);
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        return current != nullptr && current->val == key && !is_marked_ref(current);
    }

    bool add(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current != nullptr && current->val == key && !is_marked_ref(current)) {
            return false;
        }
        Node* newNode = new Node(key);
        newNode->next = current;
        while (!prev->next.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(prev->next);
            if (current != nullptr && current->val >= key) {
                delete newNode;
                return false;
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current == nullptr || current->val != key || is_marked_ref(current)) {
            return false;
        }
        Node* next = get_unmarked_ref(current->next);
        if (!current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
            return false;
        }
        while (!prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(prev->next);
            if (current == nullptr || current->val >= key) {
                return true;
            }
        }
        return true;
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

    Node* head_;
};