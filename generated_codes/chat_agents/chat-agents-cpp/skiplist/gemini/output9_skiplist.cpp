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

        Node(int v, int height) : val(v), topLevel(height) {
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
        bool found = false;
    retry:
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = pred->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* curr_unmarked = get_unmarked_ref(curr);
                if (curr_unmarked == nullptr) break;

                Node* succ = curr_unmarked->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    Node* expected = curr;
                    if (!pred->forward[i].compare_exchange_strong(expected, get_unmarked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    curr = pred->forward[i].load(std::memory_order_acquire);
                    curr_unmarked = get_unmarked_ref(curr);
                    if (curr_unmarked == nullptr) break;
                    succ = curr_unmarked->forward[i].load(std::memory_order_acquire);
                }

                if (curr_unmarked == nullptr) break;

                if (curr_unmarked->val < key) {
                    pred = curr_unmarked;
                    curr = succ;
                } else {
                    if (curr_unmarked->val == key) {
                        found = true;
                    }
                    break;
                }
            }
            preds[i] = pred;
            succs[i] = get_unmarked_ref(curr);
        }
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
                Node* curr_unmarked = get_unmarked_ref(curr);
                if (curr_unmarked == nullptr) break;

                Node* succ = curr_unmarked->forward[i].load(std::memory_order_acquire);
                while (is_marked_ref(succ)) {
                    curr = get_unmarked_ref(succ);
                    curr_unmarked = curr;
                    if (curr_unmarked == nullptr) break;
                    succ = curr_unmarked->forward[i].load(std::memory_order_acquire);
                }

                if (curr_unmarked == nullptr) break;

                if (curr_unmarked->val < key) {
                    pred = curr_unmarked;
                    curr = succ;
                } else {
                    if (curr_unmarked->val == key) {
                        return !is_marked_ref(curr_unmarked->forward[0].load(std::memory_order_acquire));
                    }
                    break;
                }
            }
        }
        return false;
    }

    bool add(int key) override {
        int height = random_level();
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

            Node* new_node = new Node(key, height);
            for (int i = 0; i < height; ++i) {
                new_node->forward[i].store(succs[i], std::memory_order_relaxed);
            }

            Node* pred = preds[0];
            Node* succ = succs[0];
            if (pred->forward[0].compare_exchange_strong(succ, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                for (int i = 1; i < height; ++i) {
                    while (true) {
                        pred = preds[i];
                        succ = succs[i];
                        new_node->forward[i].store(succ, std::memory_order_relaxed);
                        if (pred->forward[i].compare_exchange_strong(succ, new_node, std::memory_order_acq_rel, std::memory_order_acquire)) {
                            break;
                        }
                        find(key, preds, succs);
                    }
                }
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }

            Node* node_to_remove = succs[0];
            int height = node_to_remove->topLevel;

            for (int i = height - 1; i >= 0; --i) {
                Node* succ = node_to_remove->forward[i].load(std::memory_order_acquire);
                while (!is_marked_ref(succ)) {
                    if (node_to_remove->forward[i].compare_exchange_strong(succ, get_marked_ref(succ), std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    succ = node_to_remove->forward[i].load(std::memory_order_acquire);
                }
            }

            Node* succ = node_to_remove->forward[0].load(std::memory_order_acquire);
            if (is_marked_ref(succ)) {
                find(key, preds, succs);
                return true;
            }
        }
    }
};
