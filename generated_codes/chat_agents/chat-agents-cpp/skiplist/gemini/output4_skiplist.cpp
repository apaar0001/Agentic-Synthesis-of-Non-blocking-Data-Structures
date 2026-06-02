#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
public:
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int height) : val(v), topLevel(height) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

private:
    Node* head;
    Node* tail;

    int random_level() {
        static thread_local std::mt19937 gen(std::random_device{}());
        static thread_local std::geometric_distribution<int> dist(0.5);
        int lvl = dist(gen);
        if (lvl >= MAX_LEVEL) lvl = MAX_LEVEL - 1;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        bool found = false;
    retry:
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* succ = curr;
                while (is_marked_ref(succ)) {
                    Node* unmarked_curr = get_unmarked_ref(curr);
                    Node* succ_unmarked = unmarked_curr->forward[i].load(std::memory_order_acquire);
                    if (!pred->forward[i].compare_exchange_strong(curr, succ_unmarked, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = succ_unmarked;
                    succ = curr;
                }
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = unmarked_curr->forward[i].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
            preds[i] = pred;
            succs[i] = get_unmarked_ref(curr);
        }
        found = (succs[0]->val == key);
        return found;
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                curr = get_unmarked_ref(curr);
                if (curr->val < key) {
                    pred = curr;
                    curr = curr->forward[i].load(std::memory_order_acquire);
                } else {
                    break;
                }
            }
        }
        Node* curr = get_unmarked_ref(pred->forward[0].load(std::memory_order_acquire));
        return (curr->val == key && !is_marked_ref(curr->forward[0].load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        int topLevel = random_level();
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }

            Node* newNode = new Node(key, topLevel + 1);
            for (int i = 0; i <= topLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }

            Node* pred = preds[0];
            Node* succ = succs[0];
            if (pred->forward[0].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel, std::memory_order_acquire)) {
                for (int i = 1; i <= topLevel; ++i) {
                    while (true) {
                        pred = preds[i];
                        succ = succs[i];
                        newNode->forward[i].store(succ, std::memory_order_relaxed);
                        if (pred->forward[i].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            break;
                        }
                        find(key, preds, succs);
                    }
                }
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }

            Node* nodeToRemove = succs[0];
            for (int i = nodeToRemove->topLevel - 1; i >= 1; --i) {
                Node* succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    nodeToRemove->forward[i].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire);
                    succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                }
            }

            Node* succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            while (!is_marked_ref(succ)) {
                if (nodeToRemove->forward[0].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    find(key, preds, succs);
                    return true;
                }
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            }
            return false;
        }
    }
};
