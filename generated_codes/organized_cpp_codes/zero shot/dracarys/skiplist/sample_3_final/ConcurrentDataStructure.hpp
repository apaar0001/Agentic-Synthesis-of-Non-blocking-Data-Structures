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

        Node(int val, int topLevel) : val(val), topLevel(topLevel) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i] = new Node(INT_MAX, MAX_LEVEL);
        }
    }

    ~ConcurrentDataStructure() override {
        Node* node = head;
        while (node != nullptr) {
            Node* next = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
            delete node;
            node = next;
        }
    }

    bool contains(int key) override {
        Node* node = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            while (next != nullptr && next->val < key) {
                node = next;
                next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            }
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        return node != nullptr && node->val == key;
    }

    bool add(int key) override {
        Node* newNode = new Node(key, randomLevel());
        Node* update[MAX_LEVEL];
        Node* node = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            while (next != nullptr && next->val < key) {
                node = next;
                next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            }
            update[i] = node;
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        if (node != nullptr && node->val == key) {
            return false;
        }
        for (int i = 0; i < newNode->topLevel; ++i) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < newNode->topLevel; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* node = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            while (next != nullptr && next->val < key) {
                node = next;
                next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            }
            update[i] = node;
        }
        node = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
        if (node == nullptr || node->val != key) {
            return false;
        }
        for (int i = 0; i < node->topLevel; ++i) {
            Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            while (!update[i]->forward[i].compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            }
        }
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            while (next != nullptr && is_marked_ref(next)) {
                next = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
            }
            if (next == nullptr || next->val > key) {
                break;
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

    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        return dis(gen);
    }

    Node* head;
};