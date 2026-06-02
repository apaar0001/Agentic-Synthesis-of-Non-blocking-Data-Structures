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
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i] = get_marked_ref(new Node(INT_MAX, MAX_LEVEL));
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    break;
                }
                current = next;
            }
        }
        Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_acquire));
        return next != nullptr && next->val == key && !is_marked_ref(next);
    }

    bool add(int key) {
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    update[i] = current;
                    break;
                }
                current = next;
            }
        }
        Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_acquire));
        if (next != nullptr && next->val == key && !is_marked_ref(next)) {
            return false;
        }
        int topLevel = randomLevel();
        if (topLevel > MAX_LEVEL - 1) {
            topLevel = MAX_LEVEL - 1;
        }
        Node* newNode = new Node(key, topLevel);
        for (int i = 0; i < topLevel; ++i) {
            newNode->forward[i] = get_marked_ref(update[i]->forward[i].load(std::memory_order_relaxed));
        }
        for (int i = 0; i < topLevel; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = get_marked_ref(update[i]->forward[i].load(std::memory_order_relaxed));
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    update[i] = current;
                    break;
                }
                current = next;
            }
        }
        Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_acquire));
        if (next == nullptr || next->val != key || is_marked_ref(next)) {
            return false;
        }
        Node* markedNext = get_marked_ref(next);
        for (int i = 0; i < next->topLevel; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                next = get_unmarked_ref(update[i]->forward[i].load(std::memory_order_relaxed));
                if (next == nullptr || next->val != key || is_marked_ref(next)) {
                    return false;
                }
                markedNext = get_marked_ref(next);
            }
        }
        return true;
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

    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, 2);
        int level = 1;
        while (dis(gen) == 1 && level < MAX_LEVEL) {
            ++level;
        }
        return level;
    }

private:
    Node* head;
};