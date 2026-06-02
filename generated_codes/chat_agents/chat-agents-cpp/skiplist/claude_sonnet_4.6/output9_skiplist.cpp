#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];
        std::atomic<bool> marked;
        std::atomic<bool> fullyLinked;

        Node(int v, int lvl) : val(v), topLevel(lvl), marked(false), fullyLinked(false) {
            for (int i = 0; i < MAX_LEVEL; i++)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static bool is_marked_ref(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* get_unmarked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* get_marked_ref(Node* p) { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    Node* head_;
    Node* tail_;

    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;

    int randomLevel() {
        int lvl = 1;
        while (dist_(rng_) < 0.5 && lvl < MAX_LEVEL) lvl++;
        return lvl;
    }

    int findNode(int key, Node** preds, Node** succs) {
        int lFound = -1;
    retry:
        Node* pred = head_;
        for (int lv = MAX_LEVEL - 1; lv >= 0; lv--) {
            Node* curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
            while (true) {
                Node* succ_raw = curr->forward[lv].load(std::memory_order_acquire);
                while (is_marked_ref(succ_raw)) {
                    Node* succ = get_unmarked_ref(succ_raw);
                    Node* expected = curr;
                    if (!pred->forward[lv].compare_exchange_strong(expected, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        goto retry;
                    curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
                    succ_raw = curr->forward[lv].load(std::memory_order_acquire);
                }
                if (curr->val < key) { pred = curr; curr = get_unmarked_ref(succ_raw); }
                else break;
            }
            if (lFound == -1 && curr->val == key) lFound = lv;
            preds[lv] = pred;
            succs[lv] = curr;
        }
        return lFound;
    }

public:
    ConcurrentDataStructure() : rng_(std::random_device{}()), dist_(0.0, 1.0) {
        head_ = new Node(INT_MIN, MAX_LEVEL);
        tail_ = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++)
            head_->forward[i].store(tail_, std::memory_order_relaxed);
        head_->fullyLinked.store(true, std::memory_order_relaxed);
        tail_->fullyLinked.store(true, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head_;
        while (curr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        int lFound = findNode(key, preds, succs);
        return lFound != -1
            && succs[lFound]->fullyLinked.load(std::memory_order_acquire)
            && !succs[lFound]->marked.load(std::memory_order_acquire);
    }

    bool add(int key) override {
        int topLevel = randomLevel();
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        while (true) {
            int lFound = findNode(key, preds, succs);
            if (lFound != -1) {
                Node* found = succs[lFound];
                if (!found->marked.load(std::memory_order_acquire)) {
                    while (!found->fullyLinked.load(std::memory_order_acquire)) {}
                    return false;
                }
                continue;
            }
            Node* newNode = new Node(key, topLevel);
            for (int lv = 0; lv < topLevel; lv++)
                newNode->forward[lv].store(succs[lv], std::memory_order_relaxed);
            Node* pred = preds[0], *succ = succs[0];
            Node* expected = succ;
            if (!pred->forward[0].compare_exchange_strong(expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode; continue;
            }
            for (int lv = 1; lv < topLevel; lv++) {
                while (true) {
                    pred = preds[lv]; succ = succs[lv];
                    expected = succ;
                    if (pred->forward[lv].compare_exchange_strong(expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                    findNode(key, preds, succs);
                }
            }
            newNode->fullyLinked.store(true, std::memory_order_release);
            return true;
        }
    }

    bool remove(int key) override {
        Node* nodeToRemove = nullptr;
        bool isMarked = false;
        int topLevel = -1;
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        while (true) {
            int lFound = findNode(key, preds, succs);
            if (lFound != -1) nodeToRemove = succs[lFound];
            if (!isMarked) {
                if (lFound == -1) return false;
                topLevel = nodeToRemove->topLevel;
                if (!nodeToRemove->fullyLinked.load(std::memory_order_acquire)
                    || nodeToRemove->topLevel != topLevel
                    || nodeToRemove->marked.load(std::memory_order_acquire))
                    return false;
                bool expected_bool = false;
                if (!nodeToRemove->marked.compare_exchange_strong(expected_bool, true,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                    return false;
                isMarked = true;
            }
            for (int lv = topLevel - 1; lv >= 0; lv--) {
                Node* succ = nodeToRemove->forward[lv].load(std::memory_order_acquire);
                if (!is_marked_ref(succ)) {
                    Node* marked = get_marked_ref(succ);
                    nodeToRemove->forward[lv].compare_exchange_strong(succ, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                }
                Node* expected = nodeToRemove;
                preds[lv]->forward[lv].compare_exchange_strong(expected,
                    get_unmarked_ref(nodeToRemove->forward[lv].load(std::memory_order_acquire)),
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            return true;
        }
    }
};
