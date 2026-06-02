#pragma once
#include "../utils/SetADT.hpp"
#include <climits>
#include <cstdint>
#include <atomic>
#include <random>

class ConcurrentDataStructure : public SetADT {
private:
    static const int MAX_LEVEL = 16;
    struct Node {
        int val;
        int topLevel;
        std::atomic<Node*> forward[MAX_LEVEL+1];
        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i <= MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    Node* head;
    Node* tail;
    std::atomic<int> currentMaxLevel;

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
        static thread_local std::mt19937 gen(std::random_device{}());
        static thread_local std::uniform_real_distribution<double> dis(0.0, 1.0);
        int lvl = 0;
        while (dis(gen) < 0.5 && lvl < MAX_LEVEL) {
            ++lvl;
        }
        return lvl;
    }

public:
    ConcurrentDataStructure() : currentMaxLevel(0) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; ++i) {
            head->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        Node* curr = head->forward[0].load(std::memory_order_acquire);
        while (curr != tail) {
            Node* next = curr->forward[0].load(std::memory_order_acquire);
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        Node* prev = head;
        for (int i = MAX_LEVEL; i >= 0; --i) {
            Node* curr = prev->forward[i].load(std::memory_order_acquire);
            while (true) {
                Node* unmarked = get_unmarked_ref(curr);
                bool marked = is_marked_ref(curr);
                if (marked) {
                    Node* next = unmarked->forward[i].load(std::memory_order_acquire);
                    if (prev->forward[i].compare_exchange_weak(curr, get_unmarked_ref(next),
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        curr = prev->forward[i].load(std::memory_order_acquire);
                        continue;
                    } else {
                        curr = prev->forward[i].load(std::memory_order_acquire);
                        continue;
                    }
                }
                if (!unmarked || unmarked->val >= key) {
                    break;
                }
                prev = unmarked;
                curr = unmarked->forward[i].load(std::memory_order_acquire);
            }
        }
        Node* found = prev->forward[0].load(std::memory_order_acquire);
        Node* unmarked = get_unmarked_ref(found);
        bool marked = is_marked_ref(found);
        return (!marked && unmarked && unmarked->val == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL+1];
            Node* succ[MAX_LEVEL+1];
            Node* prev = head;
            bool valid = true;
            for (int i = currentMaxLevel.load(std::memory_order_acquire); i >= 0; --i) {
                Node* curr = prev->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked = get_unmarked_ref(curr);
                    bool marked = is_marked_ref(curr);
                    if (marked) {
                        Node* next = unmarked->forward[i].load(std::memory_order_acquire);
                        if (prev->forward[i].compare_exchange_weak(curr, get_unmarked_ref(next),
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = prev->forward[i].load(std::memory_order_acquire);
                            continue;
                        } else {
                            curr = prev->forward[i].load(std::memory_order_acquire);
                            continue;
                        }
                    }
                    if (!unmarked || unmarked->val >= key) {
                        succ[i] = unmarked;
                        pred[i] = prev;
                        break;
                    }
                    prev = unmarked;
                    curr = unmarked->forward[i].load(std::memory_order_acquire);
                }
            }
            Node* found = succ[0];
            if (found && !is_marked_ref(found) && found->val == key) {
                return false;
            }
            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i) {
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            bool success = true;
            for (int i = 0; i <= lvl; ++i) {
                while (true) {
                    Node* expected = pred[i]->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(expected)) {
                        success = false;
                        break;
                    }
                    if (pred[i]->forward[i].compare_exchange_weak(expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    } else {
                        success = false;
                        break;
                    }
                }
                if (!success) break;
            }
            if (success) {
                int expectedMax = currentMaxLevel.load(std::memory_order_acquire);
                if (lvl > expectedMax) {
                    currentMaxLevel.compare_exchange_strong(expectedMax, lvl,
                            std::memory_order_release, std::memory_order_relaxed);
                }
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL+1];
            Node* succ[MAX_LEVEL+1];
            Node* prev = head;
            for (int i = currentMaxLevel.load(std::memory_order_acquire); i >= 0; --i) {
                Node* curr = prev->forward[i].load(std::memory_order_acquire);
                while (true) {
                    Node* unmarked = get_unmarked_ref(curr);
                    bool marked = is_marked_ref(curr);
                    if (marked) {
                        Node* next = unmarked->forward[i].load(std::memory_order_acquire);
                        if (prev->forward[i].compare_exchange_weak(curr, get_unmarked_ref(next),
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                            curr = prev->forward[i].load(std::memory_order_acquire);
                            continue;
                        } else {
                            curr = prev->forward[i].load(std::memory_order_acquire);
                            continue;
                        }
                    }
                    if (!unmarked || unmarked->val >= key) {
                        succ[i] = unmarked;
                        pred[i] = prev;
                        break;
                    }
                    prev = unmarked;
                    curr = unmarked->forward[i].load(std::memory_order_acquire);
                }
            }
            Node* found = succ[0];
            if (!found || is_marked_ref(found) || found->val != key) {
                return false;
            }
            for (int i = found->topLevel; i >= 0; --i) {
                while (true) {
                    Node* expected = pred[i]->forward[i].load(std::memory_order_acquire);
                    Node* expectedUnmarked = get_unmarked_ref(expected);
                    if (expectedUnmarked != found) {
                        break;
                    }
                    if (pred[i]->forward[i].compare_exchange_weak(expected, get_marked_ref(found),
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        // Node has been marked
                        break;
                    }
                }
            }
            return true;
        }
    }
};