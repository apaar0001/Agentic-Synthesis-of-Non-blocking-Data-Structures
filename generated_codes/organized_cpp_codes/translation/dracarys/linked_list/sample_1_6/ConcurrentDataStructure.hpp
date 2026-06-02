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
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        // Intentionally leak nodes for strict lock-freedom correctness
    }

    bool contains(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next)->next.load(std::memory_order_acquire);
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    continue;
                }
                current = head;
                continue;
            }
            if (next == tail) {
                return false;
            }
            if (next->val == key) {
                return true;
            }
            if (next->val > key) {
                return false;
            }
            current = next;
        }
    }

    bool add(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next)->next.load(std::memory_order_acquire);
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    continue;
                }
                current = head;
                continue;
            }
            if (next == tail) {
                return false;
            }
            if (next->val == key) {
                return false;
            }
            if (next->val > key) {
                Node* newNode = new Node(key);
                newNode->next.store(next, std::memory_order_relaxed);
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                current = head;
                continue;
            }
            current = next;
        }
    }

    bool remove(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next)->next.load(std::memory_order_acquire);
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    continue;
                }
                current = head;
                continue;
            }
            if (next == tail) {
                return false;
            }
            if (next->val == key) {
                Node* nextNext = next->next.load(std::memory_order_acquire);
                if (is_marked_ref(nextNext)) {
                    return false;
                }
                if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (current->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                        return true;
                    }
                    current = head;
                    continue;
                }
                current = head;
                continue;
            }
            current = next;
        }
    }

private:
    Node* head;
    Node* tail;
};