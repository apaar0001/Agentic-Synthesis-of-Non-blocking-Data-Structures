#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() {
        Node* min = new Node(INT_MIN);
        Node* max = new Node(INT_MAX);
        min->next = max;
        head = min;
    }

    ~ConcurrentDataStructure() override {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* current = head;
        while (true) {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            if (next->val >= key) {
                return next->val == key;
            }
            current = next;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* current = head;
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            while (true) {
                if (next->val >= key) {
                    if (next->val == key) {
                        return false;
                    }
                    Node* newNode = new Node(key);
                    newNode->next = next;
                    if (current->next.compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                    break;
                }
                current = next;
                next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* current = head;
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            while (true) {
                if (next->val >= key) {
                    if (next->val != key) {
                        return false;
                    }
                    Node* markedNext = get_marked_ref(next->next.load(std::memory_order_acquire));
                    if (next->next.compare_exchange_strong(next->next.load(std::memory_order_acquire), markedNext, std::memory_order_acq_rel)) {
                        if (current->next.compare_exchange_strong(next, next->next.load(std::memory_order_acquire), std::memory_order_acq_rel)) {
                            delete next;
                            return true;
                        }
                        next->next.compare_exchange_strong(markedNext, next->next.load(std::memory_order_acquire), std::memory_order_acq_rel);
                    }
                    break;
                }
                current = next;
                next = get_unmarked_ref(current->next.load(std::memory_order_acquire));
            }
        }
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int val) : val(val) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    std::atomic<Node*> head;
};