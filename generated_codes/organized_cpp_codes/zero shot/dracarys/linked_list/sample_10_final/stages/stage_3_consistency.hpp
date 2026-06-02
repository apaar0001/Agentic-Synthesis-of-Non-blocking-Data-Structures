#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        head_ = new Node(INT_MIN);
        tail_ = new Node(INT_MAX);
        head_->next = tail_;
    }

    ~ConcurrentDataStructure() {
        Node* current = head_;
        while (current) {
            Node* next = get_unmarked_ref(current->next);
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        return current->val == key && !is_marked_ref(current->next);
    }

    bool add(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current->val == key) return false;
        Node* node = new Node(key);
        node->next = current;
        while (!prev->next.compare_exchange_strong(current, node, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(prev->next);
            if (current->val >= key) {
                delete node;
                return false;
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* prev = head_;
        Node* current = get_unmarked_ref(prev->next);
        while (current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        if (current->val != key) return false;
        Node* next = get_unmarked_ref(current->next);
        if (is_marked_ref(next)) return false;
        while (!current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
            next = get_unmarked_ref(current->next);
            if (is_marked_ref(next)) return false;
        }
        while (!prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
            current = get_unmarked_ref(prev->next);
            if (current->val >= key) {
                return false;
            }
        }
        delete current;
        return true;
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int val) : val(val) {}
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
    Node* tail_;
};