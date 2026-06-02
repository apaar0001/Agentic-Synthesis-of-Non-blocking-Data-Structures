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

        Node(int val, int level) : val(val), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i] = nullptr;
            }
        }
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN, MAX_LEVEL)) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i] = new Node(INT_MAX, MAX_LEVEL);
        }
    }

    ~ConcurrentDataStructure() {
        Node* current = head;
        while (current != nullptr) {
            Node* next = get_unmarked_ref(current->forward[0]);
            delete current;
            current = next;
        }
    }

    bool contains(int key) {
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
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
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next->val >= key) {
                    if (next->val == key) {
                        return false;
                    }
                    update[i] = current;
                    break;
                }
                current = next;
            }
        }

        int level = randomLevel();
        Node* newNode = new Node(key, level);
        for (int i = 0; i < level; ++i) {
            newNode->forward[i] = update[i]->forward[i];
        }

        for (int i = 0; i < level; ++i) {
            while (!update[i]->forward[i].compare_exchange_strong(newNode->forward[i], newNode, std::memory_order_acq_rel)) {
                newNode->forward[i] = update[i]->forward[i];
            }
        }
        return true;
    }

    bool remove(int key) {
        Node* update[MAX_LEVEL];
        Node* current = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(current->forward[i]);
                if (next->val >= key) {
                    if (next->val == key) {
                        update[i] = current;
                        break;
                    }
                    break;
                }
                current = next;
            }
        }

        Node* nodeToRemove = get_unmarked_ref(update[0]->forward[0]);
        if (nodeToRemove->val != key) {
            return false;
        }

        for (int i = 0; i < nodeToRemove->topLevel; ++i) {
            while (true) {
                Node* next = get_unmarked_ref(update[i]->forward[i]);
                if (next == nodeToRemove) {
                    Node* markedNext = get_marked_ref(next);
                    if (!update[i]->forward[i].compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                        continue;
                    }
                    break;
                } else {
                    update[i] = get_unmarked_ref(update[i]->forward[i]);
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

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* head;
};