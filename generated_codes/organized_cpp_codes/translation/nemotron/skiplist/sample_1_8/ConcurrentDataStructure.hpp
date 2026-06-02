#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
#include <cstdint>
#include <atomic>
#include <random>
#include <array>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure();
    ~ConcurrentDataStructure() override = default;
    bool contains(int key) override;
    bool add(int key) override;
    bool remove(int key) override;

private:
    static const int MAX_LEVEL = 16;
    struct Node {
        int val;
        int topLevel;
        std::array<std::atomic<Node*>, MAX_LEVEL+1> forward;
        Node(int v, int level) : val(v), topLevel(level) {
            for (auto& ptr : forward) ptr.store(nullptr, std::memory_order_relaxed);
        }
    };

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    Node* head;
    Node* tail;
    std::atomic<int> currentLevel;
    std::mt19937 rng;
    std::uniform_real_distribution<double> dist;

    int randomLevel();
    bool find(int key, Node** pred, Node** succ);
};

ConcurrentDataStructure::ConcurrentDataStructure()
    : head(new Node(INT_MIN, MAX_LEVEL))
    , tail(new Node(INT_MAX, MAX_LEVEL))
    , currentLevel(0)
    , rng(std::random_device{}())
    , dist(0.0, 1.0)
{
    for (int i = 0; i <= MAX_LEVEL; ++i) {
        head->forward[i].store(tail, std::memory_order_relaxed);
        tail->forward[i].store(nullptr, std::memory_order_relaxed);
    }
}

int ConcurrentDataStructure::randomLevel() {
    int level = 0;
    while (dist(rng) < 0.5 && level < MAX_LEVEL) {
        ++level;
    }
    return level;
}

bool ConcurrentDataStructure::find(int key, Node** pred, Node** succ) {
    Node* x = head;
    for (int i = MAX_LEVEL; i >= 0; --i) {
        Node* next = x->forward[i].load(std::memory_order_acquire);
        while (true) {
            Node* next_unmarked = get_unmarked_ref(next);
            bool marked = is_marked_ref(next);
            if (marked) {
                Node* next_next = next_unmarked->forward[i].load(std::memory_order_acquire);
                if (x->forward[i].compare_exchange_weak(next, get_marked_ref(next_next),
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                    next = x->forward[i].load(std::memory_order_acquire);
                    continue;
                } else {
                    next = x->forward[i].load(std::memory_order_acquire);
                    continue;
                }
            }
            if (next_unmarked->val < key) {
                x = next_unmarked;
                next = x->forward[i].load(std::memory_order_acquire);
            } else {
                break;
            }
        }
        pred[i] = x;
        succ[i] = get_unmarked_ref(x->forward[i].load(std::memory_order_acquire));
    }
    return true;
}

bool ConcurrentDataStructure::contains(int key) {
    Node* pred[MAX_LEVEL+1];
    Node* succ[MAX_LEVEL+1];
    find(key, pred, succ);
    Node* node = succ[0];
    return (node != tail && !is_marked_ref(node) && node->val == key);
}

bool ConcurrentDataStructure::add(int key) {
    while (true) {
        Node* pred[MAX_LEVEL+1];
        Node* succ[MAX_LEVEL+1];
        find(key, pred, succ);
        Node* node = succ[0];
        if (node != tail && !is_marked_ref(node) && node->val == key) {
            return false;
        }
        int level = randomLevel();
        Node* newNode = new Node(key, level);
        bool valid = true;
        for (int i = 0; i <= level; ++i) {
            newNode->forward[i].store(succ[i], std::memory_order_relaxed);
        }
        for (int i = 0; i <= level; ++i) {
            if (!pred[i]->forward[i].compare_exchange_weak(succ[i], newNode,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            delete newNode;
            continue;
        }
        int oldLevel = currentLevel.load(std::memory_order_acquire);
        while (level > oldLevel &&
               !currentLevel.compare_exchange_weak(oldLevel, level,
                                                   std::memory_order_release,
                                                   std::memory_order_relaxed)) {
        }
        return true;
    }
}

bool ConcurrentDataStructure::remove(int key) {
    while (true) {
        Node* pred[MAX_LEVEL+1];
        Node* succ[MAX_LEVEL+1];
        find(key, pred, succ);
        Node* node = succ[0];
        if (node == tail || is_marked_ref(node) || node->val != key) {
            return false;
        }
        int top = node->topLevel;
        for (int i = top; i >= 0; --i) {
            Node* expected = succ[i];
            while (true) {
                Node* marked = get_marked_ref(expected);
                if (pred[i]->forward[i].compare_exchange_weak(expected, marked,
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_acquire)) {
                    break;
                } else {
                    expected = pred[i]->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(expected)) {
                        expected = get_unmarked_ref(expected);
                    }
                    if (expected != succ[i]) {
                        goto restart_remove;
                    }
                }
            }
        }
        // Node has been marked
        return true;
    restart_remove:
        continue;
    }
}