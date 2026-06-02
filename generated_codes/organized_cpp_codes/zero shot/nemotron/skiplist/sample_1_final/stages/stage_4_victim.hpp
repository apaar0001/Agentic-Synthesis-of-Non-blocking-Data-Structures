#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i <= level; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    Node* head;
    Node* tail;

    int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);
        const double p = 0.5;
        int lvl = 0;
        while (dis(gen) < p && lvl < MAX_LEVEL - 1)
            ++lvl;
        return lvl;
    }

    bool find(int key, Node* pred[MAX_LEVEL], Node* curr[MAX_LEVEL]) {
        Node* prev = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* next = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
            while (true) {
                Node* nxt = get_unmarked_ref(next->forward[level].load(std::memory_order_acquire));
                if (is_marked_ref(next->forward[level].load(std::memory_order_acquire))) {
                    // help remove marked node
                    Node* succ = get_unmarked_ref(nxt->forward[level].load(std::memory_order_acquire));
                    if (!prev->forward[level].compare_exchange_weak(next, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                    continue;
                }
                if (nxt == tail || nxt->val >= key) {
                    pred[level] = prev;
                    curr[level] = next;
                    break;
                } else {
                    prev = next;
                    next = nxt;
                }
            }
        }
        return curr[0] != nullptr && curr[0]->val == key && !is_marked_ref(curr[0]->forward[0].load(std::memory_order_acquire));
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->forward[0].load(std::memory_order_acquire));
        while (curr != tail) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        Node* pred[MAX_LEVEL];
        Node* curr[MAX_LEVEL];
        return find(key, pred, curr);
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            if (find(key, pred, succ))
                return false;

            int newLvl = randomLevel();
            Node* node = new Node(key, newLvl);
            for (int i = 0; i <= newLvl; ++i)
                node->forward[i].store(succ[i], std::memory_order_relaxed);

            bool ok = true;
            for (int i = 0; i <= newLvl; ++i) {
                Node* expected = succ[i];
                if (!pred[i]->forward[i].compare_exchange_strong(expected, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                    ok = false;
                    break;
                }
            }
            if (ok)
                return true;
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* curr[MAX_LEVEL];
            if (!find(key, pred, curr))
                return false;

            Node* node = curr[0];
            // logical removal: mark node's next pointer at level 0
            Node* next = get_unmarked_ref(node->forward[0].load(std::memory_order_acquire));
            if (!node->forward[0].compare_exchange_strong(next, get_marked_ref(next),
                std::memory_order_acq_rel, std::memory_order_acquire))
                continue;

            // physical removal: splice out node from each level
            for (int i = 0; i <= node->topLevel; ++i) {
                Node* succ = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                while (true) {
                    Node* expected = succ;
                    if (!pred[i]->forward[i].compare_exchange_strong(expected, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                }
            }
            delete node;
            return true;
        }
    }
};