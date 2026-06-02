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

    ConcurrentDataStructure() : level_(1) {
        head_ = new Node(INT_MIN, MAX_LEVEL);
        tail_ = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head_->forward[i] = tail_;
        }
    }

    ~ConcurrentDataStructure() {
        // Leaked nodes for strict lock-freedom correctness
    }

    bool contains(int key) override {
        Node* current = head_;
        for (int i = level_ - 1; i >= 0; --i) {
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
        return current->forward[0].load(std::memory_order_acquire)->val == key;
    }

    bool add(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        for (int i = level_ - 1; i >= 0; --i) {
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
        if (current->forward[0].load(std::memory_order_acquire)->val == key) {
            return false;
        }
        int newLevel = randomLevel();
        if (newLevel > level_) {
            for (int i = level_; i < newLevel; ++i) {
                update[i] = head_;
            }
            level_ = newLevel;
        }
        Node* newNode = new Node(key, newLevel);
        for (int i = 0; i < newLevel; ++i) {
            newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
        }
        for (int i = 0; i < newLevel; ++i) {
            Node* expected = get_unmarked_ref(newNode->forward[i]);
            while (!update[i]->forward[i].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                // Restart traversal
                current = head_;
                for (int j = level_ - 1; j >= 0; --j) {
                    while (true) {
                        Node* next = current->forward[j].load(std::memory_order_acquire);
                        if (is_marked_ref(next)) {
                            continue;
                        }
                        if (next->val >= key) {
                            break;
                        }
                        current = next;
                    }
                    update[j] = current;
                }
                newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
                expected = get_unmarked_ref(newNode->forward[i]);
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        for (int i = level_ - 1; i >= 0; --i) {
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
        for (int i = 0; i < level_; ++i) {
            Node* next = nodeToRemove->forward[i].load(std::memory_order_acquire);
            if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, get_marked_ref(next), std::memory_order_acq_rel)) {
                // Node has been marked
            } else {
                // Restart traversal
                current = head_;
                for (int j = level_ - 1; j >= 0; --j) {
                    while (true) {
                        Node* next = current->forward[j].load(std::memory_order_acquire);
                        if (is_marked_ref(next)) {
                            continue;
                        }
                        if (next->val >= key) {
                            break;
                        }
                        current = next;
                    }
                    update[j] = current;
                }
                nodeToRemove = current->forward[0].load(std::memory_order_acquire);
                if (nodeToRemove->val != key || is_marked_ref(nodeToRemove)) {
                    return false;
                }
            }
        }
        for (int i = level_ - 1; i >= 0; --i) {
            Node* next = nodeToRemove->forward[i].load(std::memory_order_acquire);
            if (update[i]->forward[i].compare_exchange_strong(nodeToRemove, get_unmarked_ref(next), std::memory_order_acq_rel)) {
                if (i == 0) {
                    // Physical deletion
                    delete nodeToRemove;
                }
            } else {
                // Restart traversal
                current = head_;
                for (int j = level_ - 1; j >= 0; --j) {
                    while (true) {
                        Node* next = current->forward[j].load(std::memory_order_acquire);
                        if (is_marked_ref(next)) {
                            continue;
                        }
                        if (next->val >= key) {
                            break;
                        }
                        current = next;
                    }
                    update[j] = current;
                }
                nodeToRemove = current->forward[0].load(std::memory_order_acquire);
                if (nodeToRemove->val != key || is_marked_ref(nodeToRemove)) {
                    return false;
                }
            }
        }
        while (level_ > 1 && head_->forward[level_ - 1].load(std::memory_order_acquire) == tail_) {
            --level_;
        }
        return true;
    }

private:
    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        int level = 1;
        while (dis(gen) < MAX_LEVEL / 2 && level < MAX_LEVEL) {
            ++level;
        }
        return level;
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
    int level_;
};