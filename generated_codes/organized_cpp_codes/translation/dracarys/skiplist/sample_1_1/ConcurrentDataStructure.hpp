#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int MAX_LEVEL = 16;

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

    ConcurrentDataStructure() : head_(new Node(INT_MIN, MAX_LEVEL)), tail_(new Node(INT_MAX, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head_->forward[i] = tail_;
        }
    }

    ~ConcurrentDataStructure() {
        // Leaked nodes for strict lock-freedom correctness
    }

    bool contains(int key) override {
        Node* current = head_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
        }
        Node* next = current->forward[0].load(std::memory_order_acquire);
        return !is_marked_ref(next) && next->val == key;
    }

    bool add(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
            update[i] = current;
        }
        Node* next = current->forward[0].load(std::memory_order_acquire);
        if (!is_marked_ref(next) && next->val == key) {
            return false;
        }
        int level = randomLevel();
        Node* newNode = new Node(key, level);
        for (int i = 0; i < level; ++i) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < level; ++i) {
            Node* expected = get_unmarked_ref(newNode->forward[i]);
            while (!update[i]->forward[i].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                if (is_marked_ref(expected)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
            update[i] = current;
        }
        Node* next = current->forward[0].load(std::memory_order_acquire);
        if (is_marked_ref(next) || next->val != key) {
            return false;
        }
        for (int i = 0; i < MAX_LEVEL; ++i) {
            Node* expected = get_unmarked_ref(next);
            while (!update[i]->forward[i].compare_exchange_strong(expected, get_marked_ref(next), std::memory_order_acq_rel)) {
                if (is_marked_ref(expected)) {
                    return false;
                }
            }
        }
        // Node has been marked
        for (int i = 0; i < MAX_LEVEL; ++i) {
            Node* expected = get_marked_ref(next);
            while (!update[i]->forward[i].compare_exchange_strong(expected, get_unmarked_ref(next->forward[i]), std::memory_order_acq_rel)) {
                if (is_marked_ref(expected)) {
                    return false;
                }
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

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    Node* head_;
    Node* tail_;
};