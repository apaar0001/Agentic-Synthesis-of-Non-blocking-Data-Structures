#pragma once
#include <atomic>
#include <limits>
#include <random>

class SkipListLazyDeletion {
private:
    static const int MAX_LEVEL = 32;

    struct Node {
        const int key;
        const int topLevel;
        std::atomic<uintptr_t>* next;

        Node(int k, int level) : key(k), topLevel(level) {
            next = new std::atomic<uintptr_t>[level + 1];
            for (int i = 0; i <= level; i++) {
                next[i].store(0);
            }
        }

        ~Node() {
            delete[] next;
        }
    };

    static inline uintptr_t combine(Node* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline Node* get_ptr(uintptr_t val) {
        return reinterpret_cast<Node*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    Node* head;
    Node* tail;

    int randomLevel() {
        static thread_local std::mt19937 generator;
        static thread_local std::uniform_int_distribution<int> distribution(0, 1);
        int level = 0;
        while (level < MAX_LEVEL && distribution(generator) == 0) {
            level++;
        }
        return level;
    }

    bool find(int key, Node** preds, Node** succs) {
        bool marked = false;
    retry:
        while (true) {
            Node* pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                uintptr_t curr_val = pred->next[level].load();
                Node* curr = get_ptr(curr_val);
                while (true) {
                    uintptr_t succ_val = curr->next[level].load();
                    Node* succ = get_ptr(succ_val);
                    marked = get_mark(succ_val);
                    while (marked) {
                        uintptr_t expected = combine(curr, false);
                        if (!pred->next[level].compare_exchange_strong(expected, combine(succ, false))) {
                            goto retry;
                        }
                        curr_val = pred->next[level].load();
                        curr = get_ptr(curr_val);
                        succ_val = curr->next[level].load();
                        succ = get_ptr(succ_val);
                        marked = get_mark(succ_val);
                    }
                    if (curr->key < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return succs[0]->key == key;
        }
    }

public:
    SkipListLazyDeletion() {
        head = new Node(std::numeric_limits<int>::min(), MAX_LEVEL);
        tail = new Node(std::numeric_limits<int>::max(), MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head->next[i].store(combine(tail, false));
        }
    }

    bool add(int key) {
        int topLevel = randomLevel();
        Node* preds[MAX_LEVEL + 1];
        Node* succs[MAX_LEVEL + 1];
        while (true) {
            if (find(key, preds, succs)) return false;
            Node* newNode = new Node(key, topLevel);
            for (int i = 0; i <= topLevel; i++) {
                newNode->next[i].store(combine(succs[i], false));
            }
            uintptr_t expected = combine(succs[0], false);
            if (!preds[0]->next[0].compare_exchange_strong(expected, combine(newNode, false))) {
                delete newNode;
                continue;
            }
            for (int i = 1; i <= topLevel; i++) {
                while (true) {
                    expected = combine(succs[i], false);
                    if (preds[i]->next[i].compare_exchange_strong(expected, combine(newNode, false))) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }

    bool remove(int key) {
        Node* preds[MAX_LEVEL + 1];
        Node* succs[MAX_LEVEL + 1];
        while (true) {
            if (!find(key, preds, succs)) return false;
            Node* nodeToRemove = succs[0];
            for (int level = nodeToRemove->topLevel; level >= 1; level--) {
                uintptr_t succ_val = nodeToRemove->next[level].load();
                Node* succ = get_ptr(succ_val);
                bool marked = get_mark(succ_val);
                while (!marked) {
                    uintptr_t expected = combine(succ, false);
                    nodeToRemove->next[level].compare_exchange_strong(expected, combine(succ, true));
                    succ_val = nodeToRemove->next[level].load();
                    succ = get_ptr(succ_val);
                    marked = get_mark(succ_val);
                }
            }
            uintptr_t succ_val = nodeToRemove->next[0].load();
            Node* succ = get_ptr(succ_val);
            bool marked = get_mark(succ_val);
            while (true) {
                uintptr_t expected = combine(succ, false);
                bool iMarkedIt = nodeToRemove->next[0].compare_exchange_strong(expected, combine(succ, true));
                succ_val = nodeToRemove->next[0].load();
                succ = get_ptr(succ_val);
                marked = get_mark(succ_val);
                if (iMarkedIt) {
                    return true;
                } else if (marked) {
                    return false;
                }
            }
        }
    }

    bool contains(int key) {
        Node* pred = head;
        Node* curr = nullptr;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            uintptr_t curr_val = pred->next[level].load();
            curr = get_ptr(curr_val);
            while (true) {
                uintptr_t succ_val = curr->next[level].load();
                Node* succ = get_ptr(succ_val);
                bool marked = get_mark(succ_val);
                while (marked) {
                    curr = succ;
                    succ_val = curr->next[level].load();
                    succ = get_ptr(succ_val);
                    marked = get_mark(succ_val);
                }
                if (curr->key < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr->key == key;
    }
};
