#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    static constexpr double P_FACTOR = 0.5;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    Node* head;
    Node* tail;
    std::atomic<bool> destructing{false};

    int randomLevel() {
        static thread_local std::mt19937 gen(std::random_device{}());
        static std::uniform_real_distribution<double> dist(0.0, 1.0);
        int lvl = 0;
        while (dist(gen) < P_FACTOR && lvl < MAX_LEVEL - 1) ++lvl;
        return lvl;
    }

    void find(int key, Node** pred, Node** curr) {
        Node* prev = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* next = prev->forward[level].load(std::memory_order_acquire);
            while (true) {
                next = get_unmarked_ref(next);
                if (next == tail || next->val >= key) break;
                prev = next;
                next = prev->forward[level].load(std::memory_order_acquire);
            }
            pred[level] = prev;
            curr[level] = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
        }
    }

    void helpRemove(Node* pred, int level) {
        Node* succ = pred->forward[level].load(std::memory_order_acquire);
        while (is_marked_ref(succ)) {
            Node* unmarked = get_unmarked_ref(succ);
            Node* next = unmarked->forward[level].load(std::memory_order_acquire);
            pred->forward[level].compare_exchange_weak(succ, get_marked_ref(next),
                std::memory_order_acq_rel, std::memory_order_acquire);
            succ = pred->forward[level].load(std::memory_order_acquire);
        }
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

    ~ConcurrentDataStructure() override {
        destructing.store(true, std::memory_order_release);
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
        if (destructing.load(std::memory_order_acquire)) return false;
        Node* pred[MAX_LEVEL];
        Node* curr[MAX_LEVEL];
        find(key, pred, curr);
        Node* node = get_unmarked_ref(curr[0]);
        return node != tail && node->val == key && !is_marked_ref(node->forward[0].load(std::memory_order_acquire));
    }

    bool add(int key) override {
        if (destructing.load(std::memory_order_acquire)) return false;
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* curr[MAX_LEVEL];
            find(key, pred, curr);
            Node* node = get_unmarked_ref(curr[0]);
            if (node != tail && node->val == key) {
                if (is_marked_ref(node->forward[0].load(std::memory_order_acquire))) continue;
                return false;
            }
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            bool ok = true;
            for (int level = 0; level <= newLevel; ++level) {
                newNode->forward[level].store(get_unmarked_ref(pred[level]->forward[level].load(std::memory_order_acquire)),
                    std::memory_order_relaxed);
                while (!pred[level]->forward[level].compare_exchange_weak(
                        newNode->forward[level], newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    helpRemove(pred[level], level);
                    ok = false;
                    break;
                }
                if (!ok) break;
            }
            if (ok) return true;
            delete newNode;
        }
    }

    bool remove(int key) override {
        if (destructing.load(std::memory_order_acquire)) return false;
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* curr[MAX_LEVEL];
            find(key, pred, curr);
            Node* node = get_unmarked_ref(curr[0]);
            if (node == tail || node->val != key) return false;
            if (is_marked_ref(node->forward[0].load(std::memory_order_acquire))) continue;
            bool marked = false;
            for (int level = 0; level <= node->topLevel; ++level) {
                Node* expected = node;
                Node* desired = get_marked_ref(node);
                if (pred[level]->forward[level].compare_exchange_strong(expected, desired,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    marked = true;
                } else {
                    marked = false;
                    break;
                }
            }
            if (!marked) continue;
            for (int level = node->topLevel; level >= 0; --level) {
                Node* markedNode = get_marked_ref(node);
                Node* next = node->forward[level].load(std::memory_order_acquire);
                pred[level]->forward[level].compare_exchange_strong(markedNode, get_unmarked_ref(next),
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            delete node;
            return true;
        }
    }
};