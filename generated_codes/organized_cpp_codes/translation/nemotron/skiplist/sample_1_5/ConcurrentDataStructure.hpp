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
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL + 1];
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i <= MAX_LEVEL; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    int randomLevel() {
        int lvl = 0;
        while (dist(rng) < 0.5 && lvl < MAX_LEVEL) ++lvl;
        return lvl;
    }

    void findPredecessors(int key, Node* pred[], Node* succ[]) {
        Node* prev = head;
        for (int level = MAX_LEVEL; level >= 0; --level) {
            Node* curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
            while (true) {
                Node* nextRaw = curr->forward[level].load(std::memory_order_acquire);
                if (is_marked_ref(nextRaw)) {
                    Node* next = get_unmarked_ref(nextRaw);
                    Node* nextNext = get_unmarked_ref(next->forward[level].load(std::memory_order_acquire));
                    prev->forward[level].compare_exchange_strong(nextRaw, nextNext,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    continue;
                }
                Node* next = get_unmarked_ref(nextRaw);
                if (next->val < key) {
                    prev = curr;
                    curr = next;
                } else {
                    break;
                }
            }
            pred[level] = prev;
            succ[level] = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
        }
    }

public:
    ConcurrentDataStructure() : rng(std::random_device{}()), dist(0.0, 1.0) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(tail, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        Node* curr = head->forward[0].load(std::memory_order_acquire);
        while (curr != tail) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        Node* curr = head;
        for (int level = MAX_LEVEL; level >= 0; --level) {
            Node* nextRaw;
            do {
                nextRaw = curr->forward[level].load(std::memory_order_acquire);
                if (is_marked_ref(nextRaw)) {
                    Node* next = get_unmarked_ref(nextRaw);
                    Node* nextNext = get_unmarked_ref(next->forward[level].load(std::memory_order_acquire));
                    curr->forward[level].compare_exchange_strong(nextRaw, nextNext,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                }
            } while (is_marked_ref(nextRaw));
            Node* next = get_unmarked_ref(nextRaw);
            while (next->val < key) {
                curr = next;
                do {
                    nextRaw = curr->forward[level].load(std::memory_order_acquire);
                    if (is_marked_ref(nextRaw)) {
                        Node* n = get_unmarked_ref(nextRaw);
                        Node* nn = get_unmarked_ref(n->forward[level].load(std::memory_order_acquire));
                        curr->forward[level].compare_exchange_strong(nextRaw, nn,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                } while (is_marked_ref(nextRaw));
                next = get_unmarked_ref(nextRaw);
            }
        }
        Node* nextRaw = curr->forward[0].load(std::memory_order_acquire);
        if (is_marked_ref(nextRaw)) return false;
        Node* next = get_unmarked_ref(nextRaw);
        return (next != tail && next->val == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL + 1];
            Node* succ[MAX_LEVEL + 1];
            findPredecessors(key, pred, succ);
            Node* node = get_unmarked_ref(succ[0]);
            if (node != tail && node->val == key) return false;
            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i) {
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            for (int i = 0; i <= lvl; ++i) {
                while (true) {
                    Node* expected = pred[i]->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(expected)) {
                        Node* unexp = get_unmarked_ref(expected);
                        Node* succNode = get_unmarked_ref(unexp->forward[i].load(std::memory_order_acquire));
                        pred[i]->forward[i].compare_exchange_strong(expected, succNode,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        continue;
                    }
                    if (pred[i]->forward[i].compare_exchange_strong(expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL + 1];
            Node* succ[MAX_LEVEL + 1];
            findPredecessors(key, pred, succ);
            Node* node = get_unmarked_ref(succ[0]);
            if (node == tail || node->val != key) return false;
            bool marked = true;
            for (int i = 0; i <= node->topLevel; ++i) {
                Node* expected = node;
                Node* markedPtr = get_marked_ref(node);
                if (!pred[i]->forward[i].compare_exchange_strong(expected, markedPtr,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    marked = false;
                    break;
                }
            }
            if (!marked) continue;
            // Node has been marked
            return true;
        }
    }
};