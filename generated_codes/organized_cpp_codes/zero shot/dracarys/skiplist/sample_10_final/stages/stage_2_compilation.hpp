#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
public:
    static const int MAX_LEVEL = 16;

    struct Node {
        int val;
        std::atomic<Node*> forward[MAX_LEVEL];
        int topLevel;

        Node(int val, int topLevel) : val(val), topLevel(topLevel) {
            for (int i = 0; i < MAX_LEVEL; i++) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->forward[i] = tail;
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_acquire));
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next->val >= key) {
                    break;
                }
                current = next;
            }
        }
        return current->forward[0].load(std::memory_order_acquire)->val == key;
    }

    bool add(int key) {
        Node* newNode = new Node(key, randomLevel());
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next->val >= key) {
                    break;
                }
                current = next;
            }
            update[i] = current;
        }
        if (current->forward[0].load(std::memory_order_acquire)->val == key) {
            delete newNode;
            return false;
        }
        for (int i = 0; i < newNode->topLevel; i++) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < newNode->topLevel; i++) {
            while (!update[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next->val >= key) {
                    break;
                }
                current = next;
            }
            update[i] = current;
        }
        Node* nodeToRemove = current->forward[0].load(std::memory_order_acquire);
        if (nodeToRemove->val != key) {
            return false;
        }
        for (int i = 0; i < MAX_LEVEL; i++) {
            while (true) {
                Node* next = get_unmarked_ref(nodeToRemove->forward[i].load(std::memory_order_acquire));
                if (!nodeToRemove->forward[i].compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                    next = get_unmarked_ref(nodeToRemove->forward[i].load(std::memory_order_acquire));
                } else {
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_LEVEL; i++) {
            while (true) {
                Node* next = get_unmarked_ref(update[i]->forward[i].load(std::memory_order_acquire));
                if (next == nodeToRemove) {
                    update[i]->forward[i].compare_exchange_strong(next, get_unmarked_ref(nodeToRemove->forward[i].load(std::memory_order_acquire)), std::memory_order_acq_rel);
                } else {
                    break;
                }
            }
        }
        return true;
    }

private:
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    static int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        return dis(gen);
    }

    Node* head;
    Node* tail;
};