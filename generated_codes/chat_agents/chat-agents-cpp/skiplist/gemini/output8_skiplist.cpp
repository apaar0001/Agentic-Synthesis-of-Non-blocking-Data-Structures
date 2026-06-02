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
        static thread_local std::mt19937 generator(std::random_device{}());
        std::uniform_int_distribution<int> distribution(0, 1);
        int lvl = 1;
        while (distribution(generator) == 1 && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        int retry_count = 0;
    retry:
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* succ = get_unmarked_ref(curr);
                if (succ == nullptr) break;

                Node* succ_next = succ->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ_next)) {
                    Node* unmarked_succ_next = get_unmarked_ref(succ_next);
                    if (!pred->forward[i].compare_exchange_strong(curr, unmarked_succ_next, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (++retry_count > 100) {
                            goto retry;
                        }
                        goto retry;
                    }
                    curr = unmarked_succ_next;
                    succ = get_unmarked_ref(curr);
                    if (succ == nullptr) break;
                    succ_next = succ->forward[i].load(std::memory_order_acquire);
                }

                if (succ == nullptr) break;

                if (succ->val < key) {
                    pred = succ;
                    curr = succ_next;
                } else {
                    break;
                }
            }
            preds[i] = pred;
            succs[i] = get_unmarked_ref(curr);
        }
        Node* l0_succ = succs[0];
        return (l0_succ != nullptr && l0_succ != tail && l0_succ->val == key);
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
        Node* curr = get_unmarked_ref(head);
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
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr == nullptr || unmarked_curr == tail) break;

                Node* succ_next = unmarked_curr->forward[i].load(std::memory_order_acquire);
                if (is_marked_ref(succ_next)) {
                    curr = get_unmarked_ref(succ_next);
                    continue;
                }

                if (unmarked_curr->val < key) {
                    pred = unmarked_curr;
                    curr = succ_next;
                } else {
                    break;
                }
            }
        }
        Node* l0_curr = get_unmarked_ref(pred->forward[0].load(std::memory_order_acquire));
        if (l0_curr == nullptr || l0_curr == tail || l0_curr->val != key) {
            return false;
        }
        return !is_marked_ref(l0_curr->forward[0].load(std::memory_order_acquire));
    }

    bool add(int key) override {
        int topLevel = random_level();
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (find(key, preds, succs)) {
                Node* node = succs[0];
                if (!is_marked_ref(node->forward[0].load(std::memory_order_acquire))) {
                    return false;
                }
                continue;
            }

            Node* newNode = new Node(key, topLevel);
            for (int i = 0; i < topLevel; ++i) {
                newNode->forward[i].store(succs[i], std::memory_order_relaxed);
            }

            Node* pred = preds[0];
            Node* succ = succs[0];
            if (pred->forward[0].compare_exchange_strong(succ, newNode, std::memory_order_acq_rel, std::memory_order_acquire)) {
                for (int i = 1; i < topLevel; ++i) {
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
};
