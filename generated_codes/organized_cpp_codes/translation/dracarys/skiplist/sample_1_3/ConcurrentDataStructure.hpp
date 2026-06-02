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
        Node* current = head_;
        while (current != nullptr) {
            Node* next = current->forward[0].load(std::memory_order_acquire);
            delete current;
            current = next;
        }
    }

    bool contains(int key) override {
        Node* current = head_;
        for (int i = level_ - 1; i >= 0; --i) {
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
        return next->val == key && !is_marked_ref(next);
    }

    bool add(int key) override {
        int level = randomLevel();
        Node* newNode = new Node(key, level);
        Node* update[MAX_LEVEL];
        Node* current = head_;
        bool retry;
        do {
            retry = false;
            for (int i = level - 1; i >= 0; --i) {
                while (true) {
                    Node* next = current->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(next)) {
                        continue;
                    }
                    if (next->val < key) {
                        current = next;
                    } else {
                        update[i] = current;
                        break;
                    }
                }
            }
            for (int i = 0; i < level; ++i) {
                newNode->forward[i] = update[i]->forward[i].load(std::memory_order_acquire);
            }
            for (int i = 0; i < level; ++i) {
                while (true) {
                    Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                    if (next == newNode) {
                        retry = true;
                        break;
                    }
                    if (update[i]->forward[i].compare_exchange_strong(next, newNode, std::memory_order_acq_rel)) {
                        break;
                    }
                }
            }
        } while (retry);
        if (level > level_) {
            level_ = level;
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* current = head_;
        bool retry;
        do {
            retry = false;
            for (int i = level_ - 1; i >= 0; --i) {
                while (true) {
                    Node* next = current->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(next)) {
                        continue;
                    }
                    if (next->val < key) {
                        current = next;
                    } else {
                        update[i] = current;
                        break;
                    }
                }
            }
            Node* nodeToRemove = current->forward[0].load(std::memory_order_acquire);
            if (nodeToRemove->val != key || is_marked_ref(nodeToRemove)) {
                return false;
            }
            for (int i = 0; i < level_; ++i) {
                while (true) {
                    Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                    if (next == nodeToRemove) {
                        Node* markedNext = get_marked_ref(next);
                        if (update[i]->forward[i].compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                            // Node has been marked
                            break;
                        }
                    } else {
                        retry = true;
                        break;
                    }
                }
            }
            if (retry) {
                continue;
            }
            for (int i = level_ - 1; i >= 0; --i) {
                while (true) {
                    Node* next = update[i]->forward[i].load(std::memory_order_acquire);
                    if (next == nodeToRemove) {
                        Node* unmarkedNext = get_unmarked_ref(next);
                        if (update[i]->forward[i].compare_exchange_strong(next, unmarkedNext, std::memory_order_acq_rel)) {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
            while (level_ > 1 && head_->forward[level_ - 1].load(std::memory_order_acquire) == tail_) {
                --level_;
            }
            return true;
        } while (true);
    }

private:
    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<int> dis(1, MAX_LEVEL);
        int level = 1;
        while (dis(gen) % 2 == 0 && level < MAX_LEVEL) {
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