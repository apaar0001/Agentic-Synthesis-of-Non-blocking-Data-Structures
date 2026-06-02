#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int key, int height) : val(key), topLevel(height) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    Node* head;
    Node* tail;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    int random_level() {
        static thread_local std::mt19937 gen(std::random_device{}());
        static thread_local std::geometric_distribution<int> dist(0.5);
        int lvl = dist(gen);
        if (lvl >= MAX_LEVEL) lvl = MAX_LEVEL - 1;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        bool retry = false;
    RETRY:
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(curr)) {
                    retry = true;
                    goto RETRY;
                }
                Node* succ = curr->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* unmarked_succ = get_unmarked_ref(succ);
                    if (!pred->forward[i].compare_exchange_strong(curr, unmarked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        retry = true;
                        goto RETRY;
                    }
                    curr = unmarked_succ;
                    succ = curr->forward[i].load(std::memory_order_acquire);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            preds[i] = pred;
            succs[i] = curr;
        }
        return (succs[0]->val == key);
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
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
            Node* curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            while (true) {
                Node* next = curr->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(next)) {
                    curr = get_unmarked_ref(next);
                    next = curr->forward[i].load(std::memory_order_acquire);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(next);
                } else {
                    break;
                }
            }
            if (curr->val == key) {
                return true;
            }
        }
        return false;
    }

    bool add(int key) override {
        int topLevel = random_level();
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }

            Node* newNode = new Node(key, topLevel);
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
            for (int i = nodeToRemove->topLevel; i >= 0; --i) {
                Node* succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    nodeToRemove->forward[i].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire);
                    succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                }
            }

            Node* succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                find(key, preds, succs);
                return true;
            }
        }
    }
};
