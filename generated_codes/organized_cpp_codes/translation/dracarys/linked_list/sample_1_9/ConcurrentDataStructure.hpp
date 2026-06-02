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
            Node* next = head->next;
            delete head;
            head = next;
        }
    }

    bool contains(int key) {
        while (true) {
            Node* current = head;
            while (current != tail) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    current = get_unmarked_ref(next);
                } else {
                    if (current->val == key) {
                        return true;
                    }
                    if (next->val > key) {
                        return false;
                    }
                    current = next;
                }
            }
            return false;
        }
    }

    bool add(int key) {
        while (true) {
            Node* current = head;
            while (current != tail) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    current = get_unmarked_ref(next);
                } else {
                    if (current->val == key) {
                        return false;
                    }
                    if (next->val > key) {
                        Node* newNode = new Node(key);
                        newNode->next = next;
                        if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                        delete newNode;
                    }
                    current = next;
                }
            }
            return false;
        }
    }

    bool remove(int key) {
        while (true) {
            Node* current = head;
            while (current != tail) {
                Node* next = current->next.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    current = get_unmarked_ref(next);
                } else {
                    if (current->val == key) {
                        Node* nextNext = next->next.load(std::memory_order_acquire);
                        if (is_marked_ref(nextNext)) {
                            return false;
                        }
                        if (next->next.compare_exchange_strong(nextNext, get_marked_ref(nextNext), std::memory_order_acq_rel)) {
                            // Node has been marked
                            if (current->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                                return true;
                            } else {
                                // Retry if CAS fails
                                continue;
                            }
                        } else {
                            // Retry if CAS fails
                            continue;
                        }
                    }
                    if (next->val > key) {
                        return false;
                    }
                    current = next;
                }
            }
            return false;
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