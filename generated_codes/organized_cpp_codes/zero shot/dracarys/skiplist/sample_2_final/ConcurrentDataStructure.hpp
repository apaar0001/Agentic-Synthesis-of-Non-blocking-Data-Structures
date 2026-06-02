#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    static constexpr int MIN_LEVEL = 1;

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

    std::atomic<Node*> head;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_int_distribution<int> dis;

public:
    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)), gen(rd()), dis(MIN_LEVEL, MAX_LEVEL) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i] = new Node(INT_MAX, MAX_LEVEL);
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head.load(std::memory_order_relaxed);
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->forward[0].load(std::memory_order_acquire));
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* current = head.load(std::memory_order_relaxed);
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[level].load(std::memory_order_acquire));
                if (next->val >= key) {
                    if (next->val == key) {
                        return true;
                    }
                    break;
                }
                current = next;
            }
        }
        return false;
    }

    bool add(int key) {
        Node* newNode = new Node(key, dis(gen));
        Node* current = head.load(std::memory_order_relaxed);
        Node* update[MAX_LEVEL];
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[level].load(std::memory_order_acquire));
                if (next->val >= key) {
                    if (next->val == key) {
                        delete newNode;
                        return false;
                    }
                    update[level] = current;
                    break;
                }
                current = next;
            }
        }
        for (int level = 0; level < newNode->topLevel; ++level) {
            newNode->forward[level] = update[level]->forward[level].load(std::memory_order_acquire);
        }
        for (int level = 0; level < newNode->topLevel; ++level) {
            while (!update[level]->forward[level].compare_exchange_strong(newNode->forward[level], newNode, std::memory_order_acq_rel)) {
                newNode->forward[level] = update[level]->forward[level].load(std::memory_order_acquire);
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* current = head.load(std::memory_order_relaxed);
        Node* update[MAX_LEVEL];
        Node* marked = nullptr;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[level].load(std::memory_order_acquire));
                if (next->val >= key) {
                    if (next->val == key) {
                        if (is_marked_ref(next)) {
                            return false;
                        }
                        marked = next;
                        update[level] = current;
                        break;
                    }
                    update[level] = current;
                    break;
                }
                current = next;
            }
        }
        if (marked == nullptr) {
            return false;
        }
        for (int level = 0; level < marked->topLevel; ++level) {
            Node* next = get_unmarked_ref(marked->forward[level].load(std::memory_order_acquire));
            while (!marked->forward[level].compare_exchange_strong(next, get_marked_ref(next), std::memory_order_acq_rel)) {
                next = get_unmarked_ref(marked->forward[level].load(std::memory_order_acquire));
            }
        }
        for (int level = marked->topLevel - 1; level >= 0; --level) {
            while (update[level]->forward[level].load(std::memory_order_acquire) == marked) {
                update[level]->forward[level].compare_exchange_strong(marked, get_unmarked_ref(marked->forward[level].load(std::memory_order_acquire)), std::memory_order_acq_rel);
            }
        }
        delete marked;
        return true;
    }
};