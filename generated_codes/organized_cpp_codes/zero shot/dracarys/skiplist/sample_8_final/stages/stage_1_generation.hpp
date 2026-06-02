#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;

    struct Node {
        int val;
        std::atomic<Node*> forward[MAX_LEVEL];
        int topLevel;
    };

    Node* head;
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<double> dis;

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
        double r = dis(gen);
        int level = 1;
        while (r < 0.5 && level < MAX_LEVEL) {
            r *= 2;
            level++;
        }
        return level;
    }

    Node* newNode(int val, int level) {
        Node* node = new Node();
        node->val = val;
        node->topLevel = level;
        for (int i = 0; i < level; i++) {
            node->forward[i] = head;
        }
        return node;
    }

public:
    ConcurrentDataStructure() {
        head = newNode(INT_MIN, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head->forward[i] = newNode(INT_MAX, MAX_LEVEL);
        }
    }

    ~ConcurrentDataStructure() {
        Node* node = head;
        while (node != nullptr) {
            Node* next = get_unmarked_ref(node->forward[0]);
            delete node;
            node = next;
        }
    }

    bool contains(int key) {
        Node* node = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i]);
                if (next->val >= key) {
                    if (next->val == key) {
                        return true;
                    }
                    break;
                }
                node = next;
            }
        }
        return false;
    }

    bool add(int key) {
        Node* node = head;
        Node* update[MAX_LEVEL];
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i]);
                if (next->val >= key) {
                    update[i] = node;
                    if (next->val == key) {
                        return false;
                    }
                    break;
                }
                node = next;
            }
        }
        int level = randomLevel();
        Node* newNode = newNode(key, level);
        for (int i = 0; i < level; i++) {
            newNode->forward[i] = update[i]->forward[i];
            update[i]->forward[i] = newNode;
        }
        return true;
    }

    bool remove(int key) {
        Node* node = head;
        Node* update[MAX_LEVEL];
        Node* marked = nullptr;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node* next = get_unmarked_ref(node->forward[i]);
                if (next->val >= key) {
                    update[i] = node;
                    if (next->val == key) {
                        marked = next;
                        break;
                    }
                    break;
                }
                node = next;
            }
        }
        if (marked == nullptr) {
            return false;
        }
        for (int i = 0; i < marked->topLevel; i++) {
            while (true) {
                Node* next = get_unmarked_ref(marked->forward[i]);
                if (next != nullptr && is_marked_ref(next)) {
                    next = get_unmarked_ref(next);
                }
                if (update[i]->forward[i].compare_exchange_strong(marked, next, std::memory_order_acq_rel)) {
                    break;
                }
            }
        }
        return true;
    }
};