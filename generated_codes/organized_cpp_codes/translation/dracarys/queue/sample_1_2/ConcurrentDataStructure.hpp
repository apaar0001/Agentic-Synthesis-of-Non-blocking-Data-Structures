#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/QueueADT.hpp"

class ConcurrentDataStructure : public QueueADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int val) : val(val), next(nullptr) {}
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN)) {}

    ~ConcurrentDataStructure() {
        while (head) {
            Node* temp = head;
            head = head->next.load(std::memory_order_acquire);
            delete temp;
        }
    }

    void enqueue(int val) override {
        Node* newNode = new Node(val);
        while (true) {
            Node* prev = head;
            Node* curr = prev->next.load(std::memory_order_acquire);
            if (curr == nullptr) {
                if (prev->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel)) {
                    return;
                }
            } else if (is_marked_ref(curr)) {
                help(prev, curr);
                continue;
            } else {
                prev = curr;
                curr = curr->next.load(std::memory_order_acquire);
            }
        }
    }

    int dequeue() override {
        while (true) {
            Node* prev = head;
            Node* curr = prev->next.load(std::memory_order_acquire);
            if (curr == nullptr) {
                return INT_MIN;
            } else if (is_marked_ref(curr)) {
                help(prev, curr);
                continue;
            } else {
                Node* next = curr->next.load(std::memory_order_acquire);
                if (next == nullptr) {
                    return curr->val;
                } else if (is_marked_ref(next)) {
                    help(curr, next);
                    continue;
                } else {
                    Node* markedNext = get_marked_ref(next);
                    if (curr->next.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                        // Node has been marked
                        if (prev->next.compare_exchange_strong(curr, next, std::memory_order_acq_rel)) {
                            delete curr;
                            return next->val;
                        } else {
                            continue;
                        }
                    } else {
                        continue;
                    }
                }
            }
        }
    }

    bool isEmpty() override {
        Node* curr = head->next.load(std::memory_order_acquire);
        while (curr != nullptr && is_marked_ref(curr)) {
            curr = curr->next.load(std::memory_order_acquire);
        }
        return curr == nullptr;
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

    void help(Node* prev, Node* curr) {
        Node* next = curr->next.load(std::memory_order_acquire);
        if (prev->next.compare_exchange_strong(curr, next, std::memory_order_acq_rel)) {
            delete curr;
        }
    }

    Node* head;
};