#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <random>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;
    static constexpr double P_FACTOR = 0.5;

    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
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
    std::mt19937 gen;
    std::uniform_real_distribution<double> dist;

    int randomLevel() {
        int lvl = 1;
        while (dist(gen) < P_FACTOR && lvl < MAX_LEVEL) ++lvl;
        return lvl;
    }

    void find(int key, Node** update, Node** succ) {
        Node* curr = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next->val >= key) break;
                curr = next;
            }
            update[i] = curr;
            succ[i] = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
        }
    }

public:
    ConcurrentDataStructure() : gen(std::random_device{}()), dist(0.0, 1.0) {
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
        Node* curr = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            while (true) {
                Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                if (next->val >= key) break;
                curr = next;
            }
        }
        Node* next = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
        return (next->val == key && !is_marked_ref(next));
    }

    bool add(int key) override {
        Node* update[MAX_LEVEL];
        Node* succ[MAX_LEVEL];
        while (true) {
            find(key, update, succ);
            Node* candidate = get_unmarked_ref(succ[0]);
            if (candidate->val == key && !is_marked_ref(candidate)) {
                return false;
            }
            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i < lvl; ++i) {
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            bool ok = true;
            for (int i = 0; i < lvl; ++i) {
                while (true) {
                    Node* pred = update[i];
                    Node* expected = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                    if (expected != succ[i]) {
                        ok = false;
                        break;
                    }
                    if (pred->forward[i].compare_exchange_weak(expected, newNode,
                                                              std::memory_order_acq_rel,
                                                              std::memory_order_relaxed)) {
                                                                  std::this_thread::sleep_for(std::chrono::seconds(5));
                        break;
                    }
                }
                if (!ok) break;
            }
            if (ok) return true;
            // if failed, help cleanup and retry
            delete newNode;
        }
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        Node* succ[MAX_LEVEL];
        while (true) {
            find(key, update, succ);
            Node* candidate = get_unmarked_ref(succ[0]);
            if (candidate->val != key || is_marked_ref(candidate)) {
                return false;
            }
            // logical removal: mark next pointers of candidate at each level
            for (int i = 0; i < candidate->topLevel; ++i) {
                while (true) {
                    Node* next = get_unmarked_ref(candidate->forward[i].load(std::memory_order_acquire));
                    Node* marked = get_marked_ref(next);
                    if (candidate->forward[i].compare_exchange_weak(next, marked,
                                                                   std::memory_order_acq_rel,
                                                                   std::memory_order_relaxed)) {
                                                                       std::this_thread::sleep_for(std::chrono::seconds(5));
                        break;
                    }
                }
            }
            // physical removal: splice out candidate
            for (int i = candidate->topLevel - 1; i >= 0; --i) {
                while (true) {
                    Node* pred = update[i];
                    Node* curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                    if (curr != candidate) break;
                    Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                    if (pred->forward[i].compare_exchange_weak(curr, next,
                                                              std::memory_order_acq_rel,
                                                                 std::memory_order_relaxed)) {
                                                                     std::this_thread::sleep_for(std::chrono::seconds(5));
                        break;
                    }
                    // help update predecessor
                    update[i] = pred;
                }
            }
            delete candidate;
            return true;
        }
    }
};