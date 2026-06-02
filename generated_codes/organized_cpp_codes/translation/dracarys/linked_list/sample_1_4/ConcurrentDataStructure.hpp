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
        while (head != nullptr) {
            Node* next = head->next.load(std::memory_order_acquire);
            delete head;
            head = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head;
        while (curr != tail) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
                if (curr->next.compare_exchange_strong(next, next, std::memory_order_acq_rel)) {
                    continue;
                }
            }
            if (curr->val == key) {
                return true;
            }
            if (curr->next.load(std::memory_order_acquire)->val > key) {
                return false;
            }
            curr = next;
        }
        return false;
    }

    bool add(int key) override {
        Node* curr = head;
        while (curr != tail) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
                if (curr->next.compare_exchange_strong(next, next, std::memory_order_acq_rel)) {
                    continue;
                }
            }
            if (curr->val == key) {
                return false;
            }
            if (curr->next.load(std::memory_order_acquire)->val > key) {
                Node* newNode = new Node(key);
                newNode->next.store(next, std::memory_order_release);
                if (curr->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                    return true;
                }
                delete newNode;
            }
            curr = next;
        }
        return false;
    }

    bool remove(int key) override {
        Node* curr = head;
        while (curr != tail) {
            Node* next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
                if (curr->next.compare_exchange_strong(next, next, std::memory_order_acq_rel)) {
                    continue;
                }
            }
            if (curr->val == key) {
                return false;
            }
            if (curr->next.load(std::memory_order_acquire)->val > key) {
                return false;
            }
            if (next->val == key) {
                if (next->next.load(std::memory_order_acquire) == nullptr) {
                    return false;
                }
                Node* markedNext = get_marked_ref(next->next.load(std::memory_order_acquire));
                Node* expectedNext = next->next.load(std::memory_order_acquire);
                if (next->next.compare_exchange_strong(expectedNext, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    if (curr->next.compare_exchange_strong(next, next->next.load(std::memory_order_acquire), std::memory_order_acq_rel)) {
                        return true;
                    }
                }
                return false;
            }
            curr = next;
        }
        return false;
    }

private:
    Node* head;
    Node* tail;
};