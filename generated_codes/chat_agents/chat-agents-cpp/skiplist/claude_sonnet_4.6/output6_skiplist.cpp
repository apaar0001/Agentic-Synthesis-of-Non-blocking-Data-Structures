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

        Node(int v, int lvl) : val(v), topLevel(lvl) {
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
    std::bernoulli_distribution coin_;

    int randomLevel() {
        int lvl = 1;
        while (lvl < MAX_LEVEL && coin_(rng_)) lvl++;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
    retry:
        Node* pred = head_;
        for (int lv = MAX_LEVEL - 1; lv >= 0; lv--) {
            Node* curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
            while (true) {
                Node* succ_raw = curr->forward[lv].load(std::memory_order_acquire);
                while (is_marked_ref(succ_raw)) {
                    Node* snext = get_unmarked_ref(succ_raw);
                    Node* expected = curr;
                    if (!pred->forward[lv].compare_exchange_strong(expected, snext,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        goto retry;
                    curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
                    succ_raw = curr->forward[lv].load(std::memory_order_acquire);
                }
                if (curr->val < key) { pred = curr; curr = get_unmarked_ref(succ_raw); }
                else break;
            }
            preds[lv] = pred;
            succs[lv] = curr;
        }
        return succs[0]->val == key;
    }

public:
    ConcurrentDataStructure() : rng_(std::random_device{}()), coin_(0.5) {
        head_ = new Node(INT_MIN, MAX_LEVEL);
        tail_ = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++)
            head_->forward[i].store(tail_, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head_;
        while (curr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    // Optimistic: no cleanup, pure read path using acquire loads
    bool contains(int key) override {
        Node* curr = get_unmarked_ref(head_->forward[MAX_LEVEL - 1].load(std::memory_order_acquire));
        for (int lv = MAX_LEVEL - 1; lv >= 0; lv--) {
            while (true) {
                if (is_marked_ref(curr->forward[lv].load(std::memory_order_acquire))) {
                    curr = get_unmarked_ref(curr->forward[lv].load(std::memory_order_acquire));
                    continue;
                }
                if (curr->val < key)
                    curr = get_unmarked_ref(curr->forward[lv].load(std::memory_order_acquire));
                else break;
            }
        }
        return curr->val == key
            && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
    }

    bool add(int key) override {
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        int lvl = randomLevel();
        while (true) {
            if (find(key, preds, succs)) return false;
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i < lvl; i++)
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            Node* expected = succs[0];
            if (!preds[0]->forward[0].compare_exchange_strong(expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode; continue;
            }
            for (int i = 1; i < lvl; i++) {
                while (true) {
                    expected = succs[i];
                    if (preds[i]->forward[i].compare_exchange_strong(expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        while (true) {
            if (!find(key, preds, succs)) return false;
            Node* target = succs[0];
            for (int i = target->topLevel - 1; i >= 1; i--) {
                Node* succ = target->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(succ))
                    if (target->forward[i].compare_exchange_weak(succ, get_marked_ref(succ),
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
            }
            Node* succ = target->forward[0].load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(succ)) return false;
                if (target->forward[0].compare_exchange_strong(succ, get_marked_ref(succ),
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    find(key, preds, succs);
                    return true;
                }
            }
        }
    }
};
