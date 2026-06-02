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
        head->next = tail;
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* next = head->next.load(std::memory_order_relaxed);
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
        while (true) {
            Node* curr = head;
            while (curr != tail) {
                Node* next = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                    if (next == nullptr) {
                        return false;
                    }
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
    }

    bool add(int key) {
        while (true) {
            Node* curr = head;
            while (curr != tail) {
                Node* next = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                    if (next == nullptr) {
                        return false;
                    }
                }
                if (next->val == key) {
                    return false;
                }
                if (next->val > key) {
                    Node* newNode = new Node(key);
                    newNode->next = next;
                    if (curr->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                    delete newNode;
                    break;
                }
                curr = next;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* curr = head;
            while (curr != tail) {
                Node* next = curr->next.load(std::memory_order_acquire);
                while (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                    if (next == nullptr) {
                        return false;
                    }
                }
                if (next->val == key) {
                    Node* nextNext = next->next.load(std::memory_order_acquire);
                    while (is_marked_ref(nextNext)) {
                        nextNext = get_unmarked_ref(nextNext);
                        if (nextNext == nullptr) {
                            return false;
                        }
                    }
                    if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (curr->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                            return true;
                        }
                    }
                    break;
                }
                if (next->val > key) {
                    return false;
                }
                curr = next;
            }
        }
    }

private:
    Node* head;
    Node* tail;
};