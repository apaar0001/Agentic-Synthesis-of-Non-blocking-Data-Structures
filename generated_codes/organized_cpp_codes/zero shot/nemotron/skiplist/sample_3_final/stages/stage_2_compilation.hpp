#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    static constexpr double P = 0.5;

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
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist;

    int randomLevel() {
        int lvl = 1;
        while (dist(rng) < P && lvl < MAX_LEVEL)
            ++lvl;
        return lvl;
    }

    bool find(int key, Node* pred[MAX_LEVEL], Node* succ[MAX_LEVEL]) {
        retry:
        Node* prev = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
            while (true) {
                // skip logically deleted nodes
                while (curr != tail && is_marked_ref(curr->forward[0].load(std::memory_order_acquire))) {
                    Node* next = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
                    if (!prev->forward[level].compare_exchange_weak(curr, next,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        prev = head;
                        goto retry;
                    }
                    curr = get_unmarked_ref(next);
                }
                if (curr == tail || curr->val >= key) {
                    pred[level] = prev;
                    succ[level] = curr;
                    prev = curr;
                    break;
                }
                prev = curr;
                curr = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
            }
        }
        // final check that predecessors are clean
        for (int i = 0; i < MAX_LEVEL; ++i) {
            if (pred[i] != tail && is_marked_ref(pred[i]->forward[0].load(std::memory_order_acquire))) {
                goto retry;
            }
        }
        return true;
    }

public:
    ConcurrentDataStructure()
        : head(new Node(INT_MIN, MAX_LEVEL)),
          tail(new Node(INT_MAX, MAX_LEVEL)),
          rng(std::random_device{}()),
          dist(0.0, 1.0) {
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_release);
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
        Node* pred[MAX_LEVEL];
        Node* succ[MAX_LEVEL];
        find(key, pred, succ);
        Node* node = get_unmarked_ref(succ[0]);
        return node != tail && node->val == key &&
               !is_marked_ref(node->forward[0].load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            find(key, pred, succ);
            Node* node = get_unmarked_ref(succ[0]);
            if (node != tail && node->val == key)
                return false; // already present

            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i < lvl; ++i)
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);

            bool ok = true;
            for (int i = 0; i < lvl; ++i) {
                Node* expected = succ[i];
                while (!pred[i]->forward[i].compare_exchange_weak(expected, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    expected = get_unmarked_ref(pred[i]->forward[i].load(std::memory_order_acquire));
                    if (expected != succ[i]) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) break;
            }
            if (ok) return true;
            // insertion failed, retry (will help clean up via find)
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            find(key, pred, succ);
            Node* node = get_unmarked_ref(succ[0]);
            if (node == tail || node->val != key)
                return false; // not present

            // logical deletion: mark node->forward[0]
            Node* expected = succ[0];
            Node* marked = get_marked_ref(expected);
            if (!node->forward[0].compare_exchange_strong(expected, marked,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                continue; // retry

            // physical removal from each level the node appears in
            for (int i = 0; i < node->topLevel; ++i) {
                expected = get_unmarked_ref(node->forward[i].load(std::memory_order_acquire));
                while (!pred[i]->forward[i].compare_exchange_strong(expected,
                        get_unmarked_ref(node->forward[i].load(std::memory_order_acquire)),
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    expected = get_unmarked_ref(pred[i]->forward[i].load(std::memory_order_acquire));
                    // if predecessor changed, restart removal
                    if (get_unmarked_ref(pred[i]->forward[i].load(std::memory_order_acquire)) != pred[i]) {
                        goto retry_remove;
                    }
                }
            }
            return true;
        retry_remove:
            continue;
        }
    }
};