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

    std::atomic<Node*> head_;
    std::atomic<Node*> tail_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> dist_;

    int randomLevel() {
        int lvl = 1;
        while (dist_(rng_) < 0.5 && lvl < MAX_LEVEL) lvl++;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
    retry:
        Node* pred = get_unmarked_ref(head_.load(std::memory_order_acquire));
        for (int lv = MAX_LEVEL - 1; lv >= 0; lv--) {
            Node* curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr)->forward[lv].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    Node* expected = curr;
                    if (!pred->forward[lv].compare_exchange_strong(expected, get_unmarked_ref(curr),
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        goto retry;
                    curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
                    succ = get_unmarked_ref(curr)->forward[lv].load(std::memory_order_acquire);
                }
                if (get_unmarked_ref(curr)->val < key) {
                    pred = get_unmarked_ref(curr);
                    curr = get_unmarked_ref(succ);
                } else break;
            }
            preds[lv] = pred;
            succs[lv] = get_unmarked_ref(curr);
        }
        return succs[0] != get_unmarked_ref(tail_.load(std::memory_order_acquire))
            && succs[0]->val == key;
    }

public:
    ConcurrentDataStructure() : rng_(std::random_device{}()), dist_(0.0, 1.0) {
        Node* h = new Node(INT_MIN, MAX_LEVEL);
        Node* t = new Node(INT_MAX, MAX_LEVEL);
        head_.store(h, std::memory_order_relaxed);
        tail_.store(t, std::memory_order_relaxed);
        for (int i = 0; i < MAX_LEVEL; i++)
            h->forward[i].store(t, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head_.load(std::memory_order_relaxed));
        while (curr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = get_unmarked_ref(head_.load(std::memory_order_acquire));
        Node* curr = nullptr;
        for (int lv = MAX_LEVEL - 1; lv >= 0; lv--) {
            curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
            while (get_unmarked_ref(curr)->val < key) {
                pred = get_unmarked_ref(curr);
                curr = get_unmarked_ref(pred->forward[lv].load(std::memory_order_acquire));
            }
        }
        curr = get_unmarked_ref(curr);
        return curr->val == key && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire));
    }

    bool add(int key) override {
        Node* preds[MAX_LEVEL], *succs[MAX_LEVEL];
        int lvl = randomLevel();
        Node* node = new Node(key, lvl);
        while (true) {
            if (find(key, preds, succs)) { delete node; return false; }
            for (int i = 0; i < lvl; i++)
                node->forward[i].store(succs[i], std::memory_order_relaxed);
            Node* expected = succs[0];
            if (!preds[0]->forward[0].compare_exchange_strong(expected, node,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                continue;
            for (int i = 1; i < lvl; i++) {
                while (true) {
                    expected = succs[i];
                    if (preds[i]->forward[i].compare_exchange_strong(expected, node,
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
                while (!is_marked_ref(succ)) {
                    Node* marked = get_marked_ref(succ);
                    if (target->forward[i].compare_exchange_strong(succ, marked,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                }
            }
            Node* succ = target->forward[0].load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(succ)) return false;
                Node* marked = get_marked_ref(succ);
                if (target->forward[0].compare_exchange_strong(succ, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    find(key, preds, succs);
                    return true;
                }
            }
        }
    }
};
