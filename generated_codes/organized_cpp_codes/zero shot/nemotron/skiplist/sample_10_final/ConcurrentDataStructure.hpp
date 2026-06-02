#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <vector>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < level; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    Node* head;
    Node* tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static thread_local std::mt19937& rng() {
        static thread_local std::mt19937 gen(std::random_device{}());
        return gen;
    }

    int randomLevel() {
        std::bernoulli_distribution dist(0.5);
        int level = 1;
        while (dist(rng()) && level < MAX_LEVEL) ++level;
        return level;
    }

    void find(int key, std::vector<Node*>& preds, std::vector<Node*>& succs) {
        preds.assign(MAX_LEVEL, nullptr);
        succs.assign(MAX_LEVEL, nullptr);
        Node* curr = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
            while (next != tail && next->val < key) {
                curr = next;
                next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
            }
            preds[i] = curr;
            succs[i] = next;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
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
        std::vector<Node*> preds, succs;
        find(key, preds, succs);
        Node* succ0 = succs[0];
        if (succ0 == tail || succ0->val != key) {
            return false;
        }
        Node* expected = get_unmarked_ref(succ0);
        return (preds[0]->forward[0].load(std::memory_order_acquire) == expected);
    }

    bool add(int key) override {
        while (true) {
            std::vector<Node*> preds, succs;
            find(key, preds, succs);
            Node* succ0 = succs[0];
            if (succ0 != tail && succ0->val == key) {
                return false;
            }
            int newLevel = randomLevel();
            Node* newNode = new Node(key, newLevel);
            for (int i = 0; i < newLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }
            bool valid = true;
            for (int i = 0; i < newLevel; ++i) {
                Node* pred = preds[i];
                Node* succ = succs[i];
                while (true) {
                    Node* predNext = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                    if (predNext != succ) {
                        valid = false;
                        break;
                    }
                    if (pred->forward[i].compare_exchange_weak(succ, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
                if (!valid) break;
            }
            if (valid) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            std::vector<Node*> preds, succs;
            find(key, preds, succs);
            Node* victim = succs[0];
            if (victim == tail || victim->val != key) {
                return false;
            }
            // Logically mark victim at level 0
            Node* succ0 = get_unmarked_ref(victim->forward[0].load(std::memory_order_acquire));
            while (true) {
                Node* markedSucc = get_marked_ref(succ0);
                if (victim->forward[0].compare_exchange_weak(succ0, markedSucc,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    break;
                }
                succ0 = get_unmarked_ref(victim->forward[0].load(std::memory_order_acquire));
            }
            // Physically remove victim from higher levels
            for (int i = 1; i < victim->topLevel; ++i) {
                Node* pred = preds[i];
                if (pred == nullptr) continue;
                Node* succ = get_unmarked_ref(victim->forward[i].load(std::memory_order_acquire));
                while (true) {
                    Node* predNext = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                    if (predNext != victim) {
                        break;
                    }
                    if (pred->forward[i].compare_exchange_weak(victim, succ,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            return true;
        }
    }
};