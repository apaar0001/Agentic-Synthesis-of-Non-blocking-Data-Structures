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
        static thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
        int lvl = 1;
        while (dist(gen) < 0.5 && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        Node* pred = nullptr;
        Node* curr = nullptr;
        Node* succ = nullptr;

    retry:
        while (true) {
            pred = head;
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                while (true) {
                    Node* succ_ref = curr->forward[i].load(std::memory_order_acquire);
                    while (is_marked_ref(succ_ref)) {
                        succ = get_unmarked_ref(succ_ref);
                        if (!pred->forward[i].compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            goto retry;
                        }
                        curr = succ;
                        succ_ref = curr->forward[i].load(std::memory_order_acquire);
                    }
                    if (curr->val < key) {
                        pred = curr;
                        curr = get_unmarked_ref(succ_ref);
                    } else {
                        break;
                    }
                }
                preds[i] = pred;
                succs[i] = curr;
            }
            return (curr->val == key);
        }
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
        Node* curr = get_unmarked_ref(head->forward[0].load(std::memory_order_relaxed));
        while (curr != tail && curr != nullptr) {
            Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
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
            for (int i = 0; i < topLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }

            Node* pred = preds[0];
            Node* succ = succs[0];
            if (!pred->forward[0].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }

            for (int i = 1; i < topLevel; ++i) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    Node* expected = succ;
                    newNode->forward[i].store(expected, std::memory_order_relaxed);
                    if (pred->forward[i].compare_exchange_strong(expected, newNode, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* succ = nullptr;

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }

            Node* nodeToRemove = succs[0];
            for (int i = nodeToRemove->topLevel - 1; i >= 1; --i) {
                bool marked = false;
                succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    nodeToRemove->forward[i].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire);
                    succ = nodeToRemove->forward[i].load(std::memory_order_acquire);
                }
            }

            succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            while (true) {
                if (is_marked_ref(succ)) {
                    return false;
                }
                if (nodeToRemove->forward[0].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                    find(key, preds, succs);
                    return true;
                }
                succ = nodeToRemove->forward[0].load(std::memory_order_acquire);
            }
        }
    }

    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            while (true) {
                Node* succ_ref = curr->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ_ref)) {
                    curr = get_unmarked_ref(succ_ref);
                    succ_ref = curr->forward[i].load(std::memory_order_acquire);
                }
                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ_ref);
                } else {
                    break;
                }
            }
        }
        return (curr->val == key);
    }
};
