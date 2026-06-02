#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
public:
    static const int MAX_LEVEL = 16;

    struct Node {
        int val;
        std::atomic<Node*> forward[MAX_LEVEL];
        int topLevel;

        Node(int val, int level) : val(val), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i] = nullptr;
            }
        }
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

    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)), tail(new Node(INT_MAX, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i] = tail;
        }
    }

    ~ConcurrentDataStructure() {
        Node* node = head;
        while (node != nullptr) {
            Node* next = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
            delete node;
            node = next;
        }
    }

    bool contains(int key) {
        Node* node = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    break;
                }
                node = next;
            }
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        return node != nullptr && node->val == key;
    }

    bool add(int key) {
        Node* node = head;
        Node* update[MAX_LEVEL];
        Node* predecessors[MAX_LEVEL];
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    update[i] = node;
                    predecessors[i] = node;
                    break;
                }
                node = next;
            }
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        if (node != nullptr && node->val == key) {
            return false;
        }
        int level = randomLevel();
        Node* newNode = new Node(key, level);
        for (int i = 0; i < level; ++i) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < level; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(predecessors[i], newNode, std::memory_order_acq_rel)) {
                for (int j = 0; j < i; ++j) {
                    update[j] = predecessors[j];
                    predecessors[j] = get_unmarked_ref(update[j]->forward[j].load(std::memory_order_acquire));
                }
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* node = head;
        Node* update[MAX_LEVEL];
        Node* predecessors[MAX_LEVEL];
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || next->val >= key) {
                    update[i] = node;
                    predecessors[i] = node;
                    break;
                }
                node = next;
            }
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        if (node == nullptr || node->val != key) {
            return false;
        }
        for (int i = 0; i < node->topLevel; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(predecessors[i], get_marked_ref(node->forward[i].load(std::memory_order_acquire)), std::memory_order_acq_rel)) {
                for (int j = 0; j < i; ++j) {
                    update[j] = predecessors[j];
                    predecessors[j] = get_unmarked_ref(update[j]->forward[j].load(std::memory_order_acquire));
                }
            }
        }
        for (int i = 0; i < MAX_LEVEL; ++i) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                if (next == nullptr || is_marked_ref(next)) {
                    break;
                }
                node->forward[i].compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel);
            }
        }
        return true;
    }

private:
    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        return dis(gen);
    }

    Node* head;
    Node* tail;
};