#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
#include <cstdint>
#include <atomic>
#include <vector>
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
    std::atomic<int> currentMaxLevel;

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
        static thread_local std::mt19937 rng(std::random_device{}());
        static std::bernoulli_distribution dist(0.5);
        int lvl = 0;
        while (dist(rng) && lvl < MAX_LEVEL) ++lvl;
        return lvl;
    }

    void find(int key, std::vector<Node*>& preds, std::vector<Node*>& succs) {
        preds.assign(MAX_LEVEL + 1, nullptr);
        succs.assign(MAX_LEVEL + 1, nullptr);
        Node* pred = head;
        for (int level = currentMaxLevel.load(std::memory_order_acquire); level >= 0; --level) {
            Node* curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                while (is_marked_ref(curr)) {
                    Node* succ = get_unmarked_ref(curr)->forward[level].load(std::memory_order_acquire);
                    pred->forward[level].compare_exchange_strong(curr, succ,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = pred->forward[level].load(std::memory_order_acquire);
                }
                Node* succ = get_unmarked_ref(curr)->forward[level].load(std::memory_order_acquire);
                if (!succ || succ->val >= key) {
                    preds[level] = pred;
                    succs[level] = succ;
                    break;
                } else {
                    pred = curr;
                    curr = succ;
                }
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
        currentMaxLevel.store(0, std::memory_order_relaxed);
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
        std::vector<Node*> preds(MAX_LEVEL + 1);
        std::vector<Node*> succs(MAX_LEVEL + 1);
        find(key, preds, succs);
        Node* cand = succs[0];
        if (cand == tail) return false;
        if (is_marked_ref(cand)) return false;
        return (cand->val == key);
    }

    bool add(int key) override {
        while (true) {
            std::vector<Node*> preds(MAX_LEVEL + 1);
            std::vector<Node*> succs(MAX_LEVEL + 1);
            find(key, preds, succs);
            Node* cand = succs[0];
            if (cand != tail && !is_marked_ref(cand) && cand->val == key) {
                return false;
            }
            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            bool ok = true;
            for (int i = 0; i <= lvl; ++i) {
                while (true) {
                    Node* pred = preds[i];
                    Node* succ = succs[i];
                    Node* expected = succ;
                    if (pred->forward[i].compare_exchange_strong(expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    } else {
                        if (!find(key, preds, succs)) {
                            ok = false;
                            break;
                        }
                    }
                }
                if (!ok) break;
            }
            if (!ok) {
                delete newNode;
                continue;
            }
            int oldMax = currentMaxLevel.load(std::memory_order_acquire);
            while (lvl > oldMax) {
                if (currentMaxLevel.compare_exchange_weak(oldMax, lvl,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                    break;
                oldMax = currentMaxLevel.load(std::memory_order_acquire);
            }
            return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            std::vector<Node*> preds(MAX_LEVEL + 1);
            std::vector<Node*> succs(MAX_LEVEL + 1);
            if (!find(key, preds, succs)) {
                return false;
            }
            Node* cand = succs[0];
            if (cand == tail || is_marked_ref(cand) || cand->val != key) {
                return false;
            }
            Node* expected = cand;
            Node* marked = get_marked_ref(expected);
            if (preds[0]->forward[0].compare_exchange_strong(expected, marked,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
            // Check if the key is still present after CAS failure
            std::vector<Node*> preds2(MAX_LEVEL + 1);
            std::vector<Node*> succs2(MAX_LEVEL + 1);
            if (!find(key, preds2, succs2)) {
                return true;
            }
        }
    }
};