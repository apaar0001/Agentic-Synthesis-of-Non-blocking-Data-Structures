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

    static bool is_marked_ref(Node* ref) {
        return reinterpret_cast<uintptr_t>(ref) & 1;
    }

    static Node* get_unmarked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) & ~1);
    }

    static Node* get_marked_ref(Node* ref) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ref) | 1);
    }

    ConcurrentDataStructure() : head_(new Node(INT_MIN, MAX_LEVEL)), tail_(new Node(INT_MAX, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head_->forward[i] = tail_;
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head_;
        while (current != nullptr) {
            Node* next = current->forward[0].load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                next = get_unmarked_ref(next);
            }
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* current = head_;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
        }
        Node* next = current->forward[0].load(std::memory_order_acquire);
        if (is_marked_ref(next)) {
            next = get_unmarked_ref(next);
        }
        return next->val == key;
    }

    bool add(int key) override {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        int level = dis(gen);

        Node* newNode = new Node(key, level);
        Node* update[MAX_LEVEL];
        Node* current = head_;

        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
            update[i] = current;
        }

        for (int i = 0; i < level; ++i) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }

        for (int i = 0; i < level; ++i) {
            Node* expected = newNode->forward[i];
            while (!update[i]->forward[i].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
                expected = newNode->forward[i];
            }
        }

        return newNode->val != key;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;

        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = current->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val < key) {
                    current = next;
                } else {
                    break;
                }
            }
            update[i] = current;
        }

        Node* nodeToRemove = current->forward[0].load(std::memory_order_acquire);
        if (is_marked_ref(nodeToRemove)) {
            return false;
        }
        if (nodeToRemove->val != key) {
            return false;
        }

        for (int i = 0; i < MAX_LEVEL; ++i) {
            while (true) {
                Node* next = nodeToRemove->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, get_marked_ref(next), std::memory_order_acq_rel)) {
                    break;
                }
                update[i] = update[i]->forward[i].load(std::memory_order_acquire);
            }
        }

        // Node has been marked

        for (int i = 0; i < MAX_LEVEL; ++i) {
            while (true) {
                Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (next->val < key) {
                    update[i] = next;
                } else {
                    break;
                }
            }
            if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, next, std::memory_order_acq_rel)) {
                break;
            }
        }

        return true;
    }

private:
    Node* head_;
    Node* tail_;
};