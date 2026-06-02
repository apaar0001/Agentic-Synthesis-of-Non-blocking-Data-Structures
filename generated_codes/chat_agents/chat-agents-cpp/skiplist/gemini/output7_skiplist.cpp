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
        static thread_local std::geometric_distribution<int> distribution(0.5);
        int lvl = distribution(generator);
        if (lvl >= MAX_LEVEL) return MAX_LEVEL - 1;
        return lvl;
    }

    bool find(int key, Node** preds, Node** succs) {
        int retry_count = 0;
    retry:
        Node* pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                Node* succ = curr;
                while (is_marked_ref(succ)) {
                    curr = get_unmarked_ref(succ);
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }
                
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->val >= key) {
                    preds[level] = pred;
                    succs[level] = unmarked_curr;
                    break;
                }
                
                Node* expected = curr;
                if (!pred->forward[level].compare_exchange_weak(expected, unmarked_curr, std::memory_order_acq_rel, std::memory_order_acquire)) {
                    if (++retry_count > 2) {
                        retry_count = 0;
                        goto retry;
                    }
                    goto retry;
                }
                pred = unmarked_curr;
                curr = pred->forward[level].load(std::memory_order_acquire);
            }
        }
        return (succs[0]->val == key);
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
        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = pred->forward[level].load(std::memory_order_acquire);
            while (true) {
                Node* succ = curr;
                while (is_marked_ref(succ)) {
                    curr = get_unmarked_ref(succ);
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }
                Node* unmarked_curr = get_unmarked_ref(curr);
                if (unmarked_curr->val >= key) {
                    if (unmarked_curr->val == key) {
                        Node* next_level_ref = unmarked_curr->forward[0].load(std::memory_order_acquire);
                        return !is_marked_ref(next_level_ref);
                    }
                    break;
                }
                pred = unmarked_curr;
                curr = pred->forward[level].load(std::memory_order_acquire);
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
                Node* found_node = succs[0];
                Node* next_ref = found_node->forward[0].load(std::memory_order_acquire);
                if (!is_marked_ref(next_ref)) {
                    return false;
                }
                continue;
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
            bool marked_by_me = false;

            for (int level = nodeToRemove->topLevel - 1; level >= 0; --level) {
                while (true) {
                    Node* succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                    if (is_marked_ref(succ)) {
                        break;
                    }
                    Node* marked_succ = get_marked_ref(succ);
                    if (nodeToRemove->forward[level].compare_exchange_strong(succ, marked_succ, std::memory_order_acq_rel, std::memory_order_acquire)) {
                        if (level == 0) {
                            marked_by_me = true;
                        }
                        break;
                    }
                }
            }

            if (marked_by_me) {
                find(key, preds, succs);
                return true;
            } else {
                Node* check_ref = nodeToRemove->forward[0].load(std::memory_order_acquire);
                if (is_marked_ref(check_ref)) {
                    return false;
                }
            }
        }
    }
};
