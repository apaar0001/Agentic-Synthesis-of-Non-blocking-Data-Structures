#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;
    static const double P_FACTOR;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    Node* head;
    Node* tail;

    int randomLevel() {
        thread_local static std::mt19937 gen(std::random_device{}());
        thread_local static std::uniform_real_distribution<> dis(0.0, 1.0);
        int lvl = 0;
        while (dis(gen) < P_FACTOR && lvl < MAX_LEVEL - 1)
            ++lvl;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        while (true) {
            Node* pred = head;
            bool restart = false;
            for (int level = MAX_LEVEL - 1; level >= 0; --level) {
                Node* curr = pred->forward[level].load(std::memory_order_acquire);
                while (true) {
                    Node* next = curr->forward[level].load(std::memory_order_acquire);
                    while (is_marked_ref(next)) {
                        Node* succ = get_unmarked_ref(next);
                        Node* nextNext = succ->forward[level].load(std::memory_order_acquire);
                        if (!curr->forward[level].compare_exchange_weak(
                                next, nextNext,
                                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                            restart = true;
                            break;
                        }
                        next = curr->forward[level].load(std::memory_order_acquire);
                    }
                    if (restart) break;
                    Node* succ = get_unmarked_ref(next);
                    if (succ->val < key) {
                        pred = succ;
                        curr = succ;
                        continue;
                    } else {
                        break;
                    }
                }
                if (restart) break;
                preds[level] = pred;
                succs[level] = curr;
            }
            if (!restart) {
                return (succs[0] != nullptr &&
                        get_unmarked_ref(succs[0])->val == key &&
                        !is_marked_ref(succs[0]));
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_release);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->forward[0].load(std::memory_order_acquire));
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        return find(key, preds, succs);
    }

    bool add(int key) override {
        while (true) {
            Node* preds[MAX_LEVEL];
            Node* succs[MAX_LEVEL];
            if (find(key, preds, succs)) {
                return false;
            }
            int level = randomLevel();
            Node* newNode = new Node(key, level);
            for (int i = 0; i <= level; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            bool inserted = true;
            for (int i = 0; i <= level; ++i) {
                Node* expected = succs[i];
                if (!preds[i]->forward[i].compare_exchange_strong(
                        expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    inserted = false;
                    break;
                }
            }
            if (!inserted) {
                delete newNode;
                continue;
            }
            return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* preds[MAX_LEVEL];
            Node* succs[MAX_LEVEL];
            if (!find(key, preds, succs)) {
                return false;
            }
            Node* nodeToRemove = get_unmarked_ref(succs[0]);
            bool markedAll = true;
            for (int i = 0; i <= nodeToRemove->topLevel; ++i) {
                Node* next = nodeToRemove->forward[i].load(std::memory_order_acquire);
                Node* markedNext = get_marked_ref(next);
                if (!preds[i]->forward[i].compare_exchange_strong(
                        nodeToRemove, markedNext,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    markedAll = false;
                    break;
                }
            }
            if (!markedAll) {
                continue;
            }
            return true;
        }
    }
};

constexpr double ConcurrentDataStructure::P_FACTOR = 0.5;