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
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
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
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist;

    int randomLevel() {
        int lvl = 0;
        while (dist(rng) < P_FACTOR && lvl + 1 < MAX_LEVEL) {
            ++lvl;
        }
        return lvl;
    }

    bool find(int key, Node* pred[MAX_LEVEL], Node* succ[MAX_LEVEL]) {
        Node* prev = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                while (next && is_marked_ref(curr->forward[level].load(std::memory_order_acquire))) {
                    // help remove marked node
                    Node* nextNext = get_unmarked_ref(next->forward[level].load(std::memory_order_acquire));
                    prev->forward[level].compare_exchange_strong(
                        curr, get_marked_ref(nextNext),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
                    next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                }
                if (!next || next->val >= key) {
                    pred[level] = prev;
                    succ[level] = curr;
                    break;
                }
                prev = curr;
                curr = next;
            }
        }
        return succ[0] != nullptr && succ[0]->val == key && !is_marked_ref(succ[0]->forward[0].load(std::memory_order_acquire));
    }

public:
    ConcurrentDataStructure()
        : rng(std::random_device{}()), dist(0.0, 1.0) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
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
        Node* prev = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                while (next && is_marked_ref(curr->forward[level].load(std::memory_order_acquire))) {
                    Node* nextNext = get_unmarked_ref(next->forward[level].load(std::memory_order_acquire));
                    prev->forward[level].compare_exchange_strong(
                        curr, get_marked_ref(nextNext),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
                    next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                }
                if (!next || next->val >= key) {
                    break;
                }
                prev = curr;
                curr = next;
            }
            if (curr != tail && curr->val == key && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire))) {
                return true;
            }
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            if (find(key, pred, succ)) {
                return false; // already present
            }
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            for (int i = 0; i <= newLevel; ++i) {
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            bool inserted = true;
            for (int level = 0; level <= newLevel; ++level) {
                Node* expected = succ[level];
                while (true) {
                    if (is_marked_ref(pred[level]->forward[level].load(std::memory_order_acquire))) {
                        // predecessor is marked, restart
                        inserted = false;
                        break;
                    }
                    bool success = pred[level]->forward[level].compare_exchange_strong(
                        expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    if (success) break;
                    expected = pred[level]->forward[level].load(std::memory_order_acquire);
                }
                if (!inserted) break;
            }
            if (inserted) {
                return true;
            }
            // if failed, retry
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            if (!find(key, pred, succ)) {
                return false; // not present
            }
            Node* nodeToRemove = succ[0];
            int top = nodeToRemove->topLevel;
            // logical deletion: mark node's next pointers at each level
            bool markedAll = true;
            for (int level = 0; level <= top; ++level) {
                Node* expected = nodeToRemove;
                while (true) {
                    if (is_marked_ref(pred[level]->forward[level].load(std::memory_order_acquire))) {
                        markedAll = false;
                        break;
                    }
                    bool success = pred[level]->forward[level].compare_exchange_strong(
                        expected, get_marked_ref(nodeToRemove),
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    if (success) break;
                    expected = pred[level]->forward[level].load(std::memory_order_acquire);
                }
                if (!markedAll) break;
            }
            if (!markedAll) {
                // help cleanup and retry
                continue;
            }
            // physical removal: bypass marked node
            for (int level = top; level >= 0; --level) {
                Node* succNext = get_unmarked_ref(nodeToRemove->forward[level].load(std::memory_order_acquire));
                while (true) {
                    Node* currPred = pred[level]->forward[level].load(std::memory_order_acquire);
                    if (!is_marked_ref(currPred)) {
                        // predecessor changed, need to find new pred
                        // restart removal
                        goto restart_remove;
                    }
                    bool success = pred[level]->forward[level].compare_exchange_strong(
                        get_marked_ref(nodeToRemove), succNext,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    if (success) break;
                }
            }
            delete nodeToRemove;
            return true;
        restart_remove:
            continue;
        }
    }
};