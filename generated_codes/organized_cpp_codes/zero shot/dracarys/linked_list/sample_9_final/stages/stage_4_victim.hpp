#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head(new Node(INT_MIN)) {
        head->next = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() override {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->next);
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* prev = head;
        Node* current = prev->next;
        while (current != nullptr && current->val < key) {
            prev = current;
            current = get_unmarked_ref(current->next);
        }
        return current != nullptr && current->val == key && !is_marked_ref(current->next);
    }

    bool add(int key) override {
        while (true) {
            Node* prev = head;
            Node* current = prev->next;
            while (current != nullptr && current->val < key) {
                prev = current;
                current = get_unmarked_ref(current->next);
            }
            if (current != nullptr && current->val == key) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->next = current;
            if (prev->next.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* prev = head;
            Node* current = prev->next;
            while (current != nullptr && current->val < key) {
                prev = current;
                current = get_unmarked_ref(current->next);
            }
            if (current == nullptr || current->val != key) {
                return false;
            }
            Node* next = get_unmarked_ref(current->next);
            if (current->next.compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                if (prev->next.compare_exchange_strong(current, next, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    delete current;
                    return true;
                }
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

    Node* head;
};