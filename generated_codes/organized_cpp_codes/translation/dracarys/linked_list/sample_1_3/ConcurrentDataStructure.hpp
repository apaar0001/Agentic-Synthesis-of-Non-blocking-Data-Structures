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
            Node* next = head->next.load(std::memory_order_relaxed);
            delete head;
            head = next;
        }
    }

    bool contains(int key) {
        Node* current = head;
        while (current != tail) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                current = get_unmarked_ref(next);
            } else if (current->val == key) {
                return true;
            } else {
                current = next;
            }
        }
        return false;
    }

    bool add(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                current = get_unmarked_ref(next);
            } else if (next->val >= key) {
                Node* newNode = new Node(key);
                newNode->next.store(next, std::memory_order_relaxed);
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
                current = head;
            } else {
                current = next;
            }
        }
    }

    bool remove(int key) {
        Node* current = head;
        while (true) {
            Node* next = current->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                current = get_unmarked_ref(next);
            } else if (next->val == key) {
                Node* markedNext = get_marked_ref(next);
                if (current->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    Node* nextNext = next->next.load(std::memory_order_acquire);
                    if (is_marked_ref(nextNext)) {
                        nextNext = get_unmarked_ref(nextNext);
                    }
                    if (current->next.compare_exchange_strong(markedNext, nextNext, std::memory_order_acq_rel)) {
                        return true;
                    } else {
                        // retry if CAS failed
                        current = head;
                    }
                } else {
                    // retry if CAS failed
                    current = head;
                }
            } else if (next->val > key) {
                return false;
            } else {
                current = next;
            }
        }
    }

private:
    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    Node* head;
    Node* tail;
};