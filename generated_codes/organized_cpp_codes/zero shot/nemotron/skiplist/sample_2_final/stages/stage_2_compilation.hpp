#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i <= level; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static Node* head;
    static Node* tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static int randomLevel() {
        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_real_distribution<> dis(0.0, 1.0);
        const double p = 0.5;
        int lvl = 0;
        while (dis(gen) < p && lvl < MAX_LEVEL - 1)
            ++lvl;
        return lvl;
    }

    bool find(int key, std::vector<Node*>& preds, std::vector<Node*>& succs) {
        preds.assign(MAX_LEVEL, nullptr);
        succs.assign(MAX_LEVEL, nullptr);
        Node* pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                Node* succ = curr->forward[level].load(std::memory_order_acquire);
                while (is_marked_ref(curr)) {
                    Node* next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                    pred->forward[level].compare_exchange_weak(curr, next,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = pred->forward[level].load(std::memory_order_acquire);
                }
                if (!curr || curr->val >= key) {
                    preds[level] = pred;
                    succs[level] = curr;
                    break;
                } else {
                    pred = curr;
                    curr = pred->forward[level].load(std::memory_order_acquire);
                }
            }
        }
        Node* found = get_unmarked_ref(succs[0]);
        return found && found->val == key && !is_marked_ref(found->forward[0]);
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
        std::vector<Node*> preds, succs;
        return find(key, preds, succs);
    }

    bool add(int key) override {
        while (true) {
            std::vector<Node*> preds(MAX_LEVEL), succs(MAX_LEVEL);
            if (find(key, preds, succs)) {
                return false;
            }
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            for (int i = 0; i <= newLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            bool ok = true;
            for (int level = 0; level <= newLevel; ++level) {
                Node* pred = preds[level];
                while (true) {
                    Node* succ = pred->forward[level].load(std::memory_order_acquire);
                    if (is_marked_ref(succ)) {
                        Node* next = get_unmarked_ref(succ->forward[level].load(std::memory_order_acquire));
                        pred->forward[level].compare_exchange_weak(succ, next,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                        continue;
                    }
                    if (succ != succs[level]) {
                        ok = false;
                        break;
                    }
                    if (pred->forward[level].compare_exchange_weak(succ, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    preds[level] = pred;
                    succs[level] = pred->forward[level].load(std::memory_order_acquire);
                }
                if (!ok) break;
            }
            if (ok) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            std::vector<Node*> preds(MAX_LEVEL), succs(MAX_LEVEL);
            if (!find(key, preds, succs)) {
                return false;
            }
            Node* nodeToRemove = get_unmarked_ref(succs[0]);
            bool marked = false;
            for (int level = nodeToRemove->topLevel; level >= 0; --level) {
                while (!marked) {
                    Node* succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                    if (!is_marked_ref(succ)) {
                        if (nodeToRemove->forward[level].compare_exchange_weak(succ,
                                get_marked_ref(succ), std::memory_order_acq_rel,
                                std::memory_order_acquire)) {
                            marked = true;
                        }
                    } else {
                        marked = true;
                    }
                }
                Node* pred = preds[level];
                Node* succ = get_unmarked_ref(nodeToRemove->forward[level].load(std::memory_order_acquire));
                while (pred->forward[level].load(std::memory_order_acquire) !=
                       get_marked_ref(nodeToRemove)) {
                    if (is_marked_ref(pred->forward[level].load(std::memory_order_acquire))) {
                        Node* next = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
                        pred->forward[level].compare_exchange_weak(pred->forward[load],
                            next, std::memory_order_acq_rel, std::memory_order_acquire);
                    } else {
                        pred->forward[level].compare_exchange_weak(nodeToRemove, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                }
            }
            return true;
        }
    }
};

ConcurrentDataStructure* ConcurrentDataStructure::head = nullptr;
ConcurrentDataStructure* ConcurrentDataStructure::tail = nullptr;