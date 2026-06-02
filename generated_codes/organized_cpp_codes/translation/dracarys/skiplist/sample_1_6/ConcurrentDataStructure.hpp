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
        // Manual memory management is not allowed in lock-free code
    }

    bool contains(int key) override {
        Node* current = head_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                if (next->val >= key) {
                    break;
                }
                current = next;
            }
        }
        Node* next = current->forward[0].load(std::memory_order_acquire);
        return next->val == key && !is_marked_ref(next);
    }

    bool add(int key) override {
        int level = randomLevel();
        Node* newNode = new Node(key, level);
        Node* update[MAX_LEVEL];
        Node* current = head_;
        bool added = false;
        while (!added) {
            for (int i = level - 1; i >= 0; --i) {
                while (true) {
                    Node* next = current->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(next)) {
                        continue;
                    }
                    if (next->val >= key) {
                        break;
                    }
                    current = next;
                }
                update[i] = current;
            }
            for (int i = 0; i < level; ++i) {
                newNode->forward[i].store(update[i]->forward[i].load(std::memory_order_acquire), std::memory_order_relaxed);
            }
            for (int i = 0; i < level; ++i) {
                while (true) {
                    Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(next)) {
                        continue;
                    }
                    if (next->val >= key) {
                        break;
                    }
                    if (update[i]->forward[i].compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                        added = true;
                        break;
                    }
                }
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        bool removed = false;
        while (!removed) {
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                while (true) {
                    Node* next = current->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(next)) {
                        continue;
                    }
                    if (next->val >= key) {
                        break;
                    }
                    current = next;
                }
                update[i] = current;
            }
            Node* nodeToRemove = current->forward[0].load(std::memory_order_acquire);
            if (nodeToRemove->val != key || is_marked_ref(nodeToRemove)) {
                return false;
            }
            for (int i = 0; i < MAX_LEVEL; ++i) {
                Node* next = nodeToRemove->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    continue;
                }
                Node* markedNext = get_marked_ref(next);
                if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, markedNext, std::memory_order_acq_rel)) {
                    // Node has been marked
                    removed = true;
                    break;
                }
            }
        }
        for (int i = 0; i < MAX_LEVEL; ++i) {
            while (true) {
                Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                if (next == nodeToRemove) {
                    Node* unmarkedNext = get_unmarked_ref(nodeToRemove->forward[i].load(std::memory_order_acquire));
                    if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, unmarkedNext, std::memory_order_acq_rel)) {
                        break;
                    }
                } else {
                    break;
                }
            }
        }
        return true;
    }

private:
    static int randomLevel() {
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