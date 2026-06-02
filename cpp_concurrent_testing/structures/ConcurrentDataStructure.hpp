#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

struct Node {
    int val;
    std::atomic<Node*> next;
};

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head(new Node{INT_MIN, nullptr}), tail(new Node{INT_MAX, nullptr}) {
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() {
        while (head != nullptr) {
            Node* temp = head;
            head = head->next.load(std::memory_order_acquire);
            delete temp;
        }
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool contains(int key) {
        Node* current = head;
        while (current != tail) {
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
        return false;
    }

    bool add(int key) {
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next == tail || next->val >= key) {
                    break;
                }
                current = next;
                next = next->next.load(std::memory_order_acquire);
            }
            if (next == tail || next->val > key) {
                Node* newNode = new Node{key, next};
                if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
            } else if (next->val == key) {
                return false;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* current = head;
            Node* next = current->next.load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next == tail || next->val >= key) {
                    break;
                }
                current = next;
                next = next->next.load(std::memory_order_acquire);
            }
            if (next != tail && next->val == key) {
                Node* nextNext = next->next.load(std::memory_order_acquire);
                if (is_marked_ref(nextNext)) {
                    nextNext = get_unmarked_ref(nextNext);
                }
                Node* markedNextNext = get_marked_ref(nextNext);
                if (next->next.compare_exchange_strong(nextNext, markedNextNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (current->next.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                        delete get_unmarked_ref(next);
                        return true;
                    } else {
                        next->next.store(nextNext, std::memory_order_release);
                    }
                }
            } else {
                return false;
            }
        }
    }

private:
    Node* head;
    Node* tail;
};