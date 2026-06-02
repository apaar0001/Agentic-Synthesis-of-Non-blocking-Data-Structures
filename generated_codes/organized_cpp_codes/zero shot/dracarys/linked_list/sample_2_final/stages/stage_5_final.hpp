#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)), tail(new Node(INT_MAX)) {
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* current = head.load(std::memory_order_relaxed);
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next.load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* prev = head.load(std::memory_order_relaxed);
        Node* current = prev->next.load(std::memory_order_relaxed);
        while (current != tail.load(std::memory_order_relaxed)) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current);
                continue;
            }
            if (current->val == key) {
                return true;
            } else if (current->val > key) {
                return false;
            }
            prev = current;
            current = prev->next.load(std::memory_order_relaxed);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* prev = head.load(std::memory_order_relaxed);
            Node* current = prev->next.load(std::memory_order_relaxed);
            while (current != tail.load(std::memory_order_relaxed)) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current);
                    continue;
                }
                if (current->val == key) {
                    return false;
                } else if (current->val > key) {
                    break;
                }
                prev = current;
                current = prev->next.load(std::memory_order_relaxed);
            }
            Node* newNode = new Node(key);
            newNode->next.store(current, std::memory_order_relaxed);
            if (prev->next.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* prev = head.load(std::memory_order_relaxed);
            Node* current = prev->next.load(std::memory_order_relaxed);
            while (current != tail.load(std::memory_order_relaxed)) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current);
                    continue;
                }
                if (current->val == key) {
                    Node* next = current->next.load(std::memory_order_relaxed);
                    if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                        if (prev->next.compare_exchange_strong(current, get_unmarked_ref(next), std::memory_order_acq_rel)) {
                            delete current;
                            return true;
                        }
                    }
                } else if (current->val > key) {
                    return false;
                }
                prev = current;
                current = prev->next.load(std::memory_order_relaxed);
            }
            return false;
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
    Node* tail;
};