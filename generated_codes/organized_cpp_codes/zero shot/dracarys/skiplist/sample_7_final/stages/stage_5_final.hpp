#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int MAX_LEVEL = 16;
    static constexpr int MIN_LEVEL = 1;

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
        Node* prev[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = current;
                    current = next;
                    break;
                }
                current = next;
            }
        }
        return current != nullptr && current->val == key;
    }

    bool add(int key) {
        Node* prev[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = current;
                    current = next;
                    break;
                }
                current = next;
            }
        }
        if (current != nullptr && current->val == key) {
            return false;
        }
        int topLevel = randomLevel();
        Node* newNode = new Node(key, topLevel);
        for (int i = 0; i < topLevel; i++) {
            newNode->forward[i] = prev[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < topLevel; i++) {
            while (!prev[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = prev[i]->forward[i].load(std::memory_order_acquire);
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* prev[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    prev[i] = current;
                    current = next;
                    break;
                }
                current = next;
            }
        }
        if (current == nullptr || current->val != key) {
            return false;
        }
        for (int i = 0; i < current->topLevel; i++) {
            Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
            while (!current->forward[i].compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
            }
        }
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(prev[i]->forward[i].load(std::memory_order_acquire));
                if (next == current) {
                    while (!prev[i]->forward[i].compare_exchange_strong(current, get_unmarked_ref(current->forward[i].load(std::memory_order_acquire)), std::memory_order_acq_rel)) {
                        next = get_unmarked_ref(prev[i]->forward[i].load(std::memory_order_acquire));
                    }
                } else {
                    break;
                }
            }
        }
        return true;
    }

private:
    Node* head;
    Node* tail = new Node(INT_MAX, MAX_LEVEL);

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        return dis(gen);
    }
};