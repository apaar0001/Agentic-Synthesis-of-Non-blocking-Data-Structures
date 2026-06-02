#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;
    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL + 1];
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i <= MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    Node* head;
    Node* tail;
    std::mt19937 gen;
    std::uniform_real_distribution<> dis;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    int randomLevel() {
        int level = 0;
        while (dis(gen) < 0.5 && level < MAX_LEVEL) {
            ++level;
        }
        return level;
    }

    bool find(int key, Node* pred[], Node* succ[]) {
        retry:
        Node* prev = head;
        for (int level = MAX_LEVEL; level >= 0; --level) {
            Node* curr = prev->forward[level].load(std::memory_order_acquire);
            while (true) {
                Node* curr_unmarked = get_unmarked_ref(curr);
                bool marked = is_marked_ref(curr);
                Node* succ_ptr = nullptr;
                if (!marked) {
                    succ_ptr = curr_unmarked->forward[level].load(std::memory_order_acquire);
                }
                if (marked) {
                    Node* succ_unmarked = get_unmarked_ref(succ_ptr);
                    if (prev->forward[level].compare_exchange_strong(curr, get_marked_ref(succ_unmarked),
                                                                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        curr = prev->forward[level].load(std::memory_order_acquire);
                        continue;
                    } else {
                        goto retry;
                    }
                }
                if (curr_unmarked == tail || curr_unmarked->val >= key) {
                    succ[level] = curr_unmarked;
                    pred[level] = prev;
                    break;
                } else {
                    prev = curr_unmarked;
                    curr = prev->forward[level].load(std::memory_order_acquire);
                }
            }
        }
        Node* pred_node = pred[0];
        Node* succ_node = succ[0];
        Node* actual_next = pred_node->forward[0].load(std::memory_order_acquire);
        Node* actual_next_unmarked = get_unmarked_ref(actual_next);
        bool actual_next_marked = is_marked_ref(actual_next);
        if (actual_next_marked || actual_next_unmarked != succ_node) {
            goto retry;
        }
        return (succ_node != tail && succ_node->val == key);
    }

public:
    ConcurrentDataStructure() : gen(std::random_device{}()), dis(0.0, 1.0) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, 0);
        for (int i = 0; i <= MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        Node* curr = head->forward[0].load(std::memory_order_acquire);
        while (curr != tail) {
            Node* next = curr->forward[0].load(std::memory_order_acquire);
            Node* curr_unmarked = get_unmarked_ref(curr);
            delete curr_unmarked;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        Node* pred[MAX_LEVEL + 1];
        Node* succ[MAX_LEVEL + 1];
        return find(key, pred, succ);
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL + 1];
            Node* succ[MAX_LEVEL + 1];
            if (find(key, pred, succ)) {
                return false;
            }
            int lvl = randomLevel();
            Node* node = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i) {
                node->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            bool ok = true;
            for (int i = 0; i <= lvl; ++i) {
                Node* expected = succ[i];
                if (!pred[i]->forward[i].compare_exchange_strong(expected, node,
                                                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    ok = false;
                    break;
                }
            }
            if (ok) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL + 1];
            Node* succ[MAX_LEVEL + 1];
            if (!find(key, pred, succ)) {
                return false;
            }
            Node* node = succ[0];
            Node* expected = node;
            Node* marked = get_marked_ref(node);
            if (!pred[0]->forward[0].compare_exchange_strong(expected, marked,
                                                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            for (int i = 1; i <= node->topLevel; ++i) {
                while (true) {
                    Node* curr = pred[i]->forward[i].load(std::memory_order_acquire);
                    Node* curr_unmarked = get_unmarked_ref(curr);
                    bool marked_curr = is_marked_ref(curr);
                    if (marked_curr) break;
                    if (curr_unmarked != node) break;
                    Node* desired = get_marked_ref(node);
                    if (pred[i]->forward[i].compare_exchange_strong(curr, desired,
                                                                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                }
            }
            return true;
        }
    }
};