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

    bool contains(int key) override {
        Node* current = head;
        while (current != tail) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (next == tail || next->val > key) {
                return current->val == key;
            }
            current = next;
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            while (next != tail) {
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val >= key) {
                    break;
                }
                current = next;
                next = current->next.load(std::memory_order_acquire);
            }
            if (next == tail || next->val > key) {
                if (current->val == key) {
                    return false;
                }
                Node* newNode = new Node(key);
                newNode->next.store(next, std::memory_order_relaxed);
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
            } else {
                // Skip logically deleted nodes
                continue;
            }
            continue;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            while (next != tail) {
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val >= key) {
                    break;
                }
                current = next;
                next = current->next.load(std::memory_order_acquire);
            }
            if (next == tail || next->val > key) {
                if (current->val != key) {
                    return false;
                }
                Node* markedNext = get_marked_ref(next);
                if (current->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (current->next.compare_exchange_strong(markedNext, next, std::memory_order_acq_rel)) {
                        return true;
                    }
                    break;
                }
                return false;
            } else {
                // Skip logically deleted nodes
                continue;
            }
            continue;
        }
    }

private:
    Node* head;
    Node* tail;
};