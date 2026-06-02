#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;

        Node(int val) : val(val), next(nullptr) {}
    };

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MAX)) {
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() {
        while (head) {
            Node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = get_unmarked_ref(next);
        }
    }

    bool contains(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (next == tail) {
                return false;
            }
            if (next->val >= key) {
                return next->val == key;
            }
            current = next;
        }
    }

    bool add(int key) {
        while (true) {
            Node* current = head;
            while (true) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next == tail || next->val >= key) {
                    break;
                }
                current = next;
            }
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next == tail || next->val != key) {
                Node* newNode = new Node(key);
                newNode->next.store(next, std::memory_order_relaxed);
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                return false;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* current = head;
            while (true) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next == tail || next->val >= key) {
                    break;
                }
                current = next;
            }
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next == tail || next->val != key) {
                return false;
            }
            Node* nextUnmarked = get_unmarked_ref(next->next.load(std::memory_order_acquire));
            if (next->next.compare_exchange_strong(nextUnmarked, get_marked_ref(nextUnmarked), std::memory_order_acq_rel)) {
                // Node has been marked
                if (current->next.compare_exchange_strong(next, nextUnmarked, std::memory_order_acq_rel)) {
                    return true;
                }
            }
        }
    }

private:
    Node* head;
    Node* tail;
};