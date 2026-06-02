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
        while (head) {
            Node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = next;
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
        Node* curr = head;
        while (true) {
            Node* next = curr->next.load(std::memory_order_acquire);
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
            curr = next;
        }
    }

    bool add(int key) {
        Node* curr = head;
        while (true) {
            Node* next = curr->next.load(std::memory_order_acquire);
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
                if (curr->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                curr = head;
                continue;
            }
            curr = next;
        }
    }

    bool remove(int key) {
        Node* curr = head;
        while (true) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            if (next == tail) {
                return false;
            }
            if (next->val == key) {
                Node* markedNext = get_marked_ref(next);
                if (curr->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    Node* nextNext = next->next.load(std::memory_order_acquire);
                    if (curr->next.compare_exchange_strong(markedNext, nextNext, std::memory_order_acq_rel)) {
                        delete next;
                        return true;
                    }
                    curr = nextNext;
                    continue;
                }
                return false;
            }
            if (next->val > key) {
                return false;
            }
            curr = next;
        }
    }

private:
    Node* head;
    Node* tail;
};