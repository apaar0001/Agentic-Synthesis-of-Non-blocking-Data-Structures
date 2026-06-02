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

    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MAX)) {
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* temp = head;
            head = head->next.load(std::memory_order_relaxed);
            delete temp;
        }
    }

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
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
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
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
            } else {
                current = next;
                next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (next == tail) {
                return false;
            }
            if (next->val == key) {
                Node* nextNext = next->next.load(std::memory_order_acquire);
                if (is_marked_ref(nextNext)) {
                    nextNext = get_unmarked_ref(nextNext);
                }
                if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (next->next.compare_exchange_strong(nextNext, nextNext, std::memory_order_acq_rel)) {
                        return true;
                    }
                }
            } else if (next->val > key) {
                return false;
            } else {
                current = next;
                next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
            }
        }
    }

private:
    Node* head;
    Node* tail;
};