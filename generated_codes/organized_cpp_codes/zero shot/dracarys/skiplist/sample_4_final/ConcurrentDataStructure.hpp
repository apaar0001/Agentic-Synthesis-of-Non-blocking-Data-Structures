#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int val;
        std::atomic<Node*> forward[MAX_LEVEL];
        int topLevel;

        Node(int val, int level) : val(val), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; i++) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->forward[i] = new Node(INT_MAX, MAX_LEVEL);
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->forward[0]);
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next == nullptr || next->val >= key) {
                    break;
                }
                current = next;
            }
        }
        current = get_unmarked_ref(current->forward[0]);
        return current != nullptr && current->val == key;
    }

    bool add(int key) {
        Node* newNode = new Node(key, randomLevel());
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next == nullptr || next->val >= key) {
                    break;
                }
                current = next;
            }
            update[i] = current;
        }
        current = get_unmarked_ref(current->forward[0]);
        if (current != nullptr && current->val == key) {
            delete newNode;
            return false;
        }
        for (int i = 0; i < newNode->topLevel; i++) {
            newNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i].compare_exchange_strong(current, newNode, std::memory_order_acq_rel);
        }
        return true;
    }

    bool remove(int key) {
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next == nullptr || next->val >= key) {
                    break;
                }
                current = next;
            }
            update[i] = current;
        }
        current = get_unmarked_ref(current->forward[0]);
        if (current == nullptr || current->val != key) {
            return false;
        }
        Node* marked = get_marked_ref(current);
        for (int i = 0; i < current->topLevel; i++) {
            if (update[i]->forward[i].compare_exchange_strong(current, marked, std::memory_order_acq_rel)) {
                break;
            }
        }
        for (int i = 0; i < MAX_LEVEL; i++) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next == nullptr || next->val >= key) {
                    break;
                }
                current = next;
            }
            update[i] = current;
        }
        for (int i = 0; i < current->topLevel; i++) {
            current->forward[i].compare_exchange_strong(marked, update[i]->forward[i], std::memory_order_acq_rel);
        }
        delete current;
        return true;
    }

private:
    static int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        return dis(gen);
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

    Node* head;
};