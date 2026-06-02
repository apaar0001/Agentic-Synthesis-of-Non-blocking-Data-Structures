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
        std::atomic<Node*>* forward;
        Node(int v, int level) : val(v), topLevel(level) {
            forward = new std::atomic<Node*>[level+1];
            for (int i=0;i<=level;++i) forward[i].store(nullptr, std::memory_order_relaxed);
        }
        ~Node() { delete[] forward; }
    };

    Node* header;
    Node* tail;

    std::mt19937& gen() {
        static thread_local std::mt19937 local_gen(std::random_device{}());
        return local_gen;
    }

    int randomLevel() {
        int lvl = 0;
        while ((gen()() & 1) && lvl < MAX_LEVEL-1) ++lvl;
        return lvl;
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    void find(int key, Node** pred, Node** succ) {
        Node* prev = header;
        for (int level = MAX_LEVEL; level >= 0; --level) {
            Node* next;
            while (true) {
                next = prev->forward[level].load(std::memory_order_acquire);
                Node* unmarked = get_unmarked_ref(next);
                if (is_marked_ref(next)) {
                    Node* nextNext = unmarked->forward[level].load(std::memory_order_acquire);
                    prev->forward[level].compare_exchange_weak(next, nextNext,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                    continue;
                }
                if (unmarked == tail || unmarked->val >= key) break;
                prev = unmarked;
            }
            pred[level] = prev;
            succ[level] = get_unmarked_ref(prev->forward[level].load(std::memory_order_acquire));
        }
    }

public:
    ConcurrentDataStructure() {
        header = new Node(INT_MIN, MAX_LEVEL);
        tail   = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; ++i) {
            header->forward[i].store(tail, std::memory_order_relaxed);
            tail->forward[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        delete header;
        delete tail;
    }

    bool contains(int key) override {
        Node* pred[MAX_LEVEL+1];
        Node* succ[MAX_LEVEL+1];
        find(key, pred, succ);
        Node* node = succ[0];
        return node != tail && node->val == key && !is_marked_ref(succ[0]);
    }

    bool add(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL+1];
            Node* succ[MAX_LEVEL+1];
            find(key, pred, succ);
            Node* node = succ[0];
            if (node != tail && node->val == key) {
                if (is_marked_ref(succ[0])) {
                    // treat marked as absent
                } else {
                    return false;
                }
            }
            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i) {
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);
            }
            if (!pred[0]->forward[0].compare_exchange_weak(succ[0], newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;
            }
            for (int i = 1; i <= lvl; ++i) {
                while (true) {
                    Node* currPred = pred[i];
                    Node* currSucc = succ[i];
                    Node* currNext = currPred->forward[i].load(std::memory_order_acquire);
                    if (get_unmarked_ref(currNext) != currSucc || is_marked_ref(currNext)) {
                        if (is_marked_ref(currNext)) {
                            Node* unmarked = get_unmarked_ref(currNext);
                            currPred->forward[i].compare_exchange_weak(currNext, unmarked,
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        }
                        break;
                    }
                    if (currPred->forward[i].compare_exchange_weak(currSucc, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    Node* curr = currPred->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(curr)) {
                        Node* unmarked = get_unmarked_ref(curr);
                        currPred->forward[i].compare_exchange_weak(curr, unmarked,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                }
            }
            return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred[MAX_LEVEL+1];
            Node* succ[MAX_LEVEL+1];
            find(key, pred, succ);
            Node* node = succ[0];
            if (node == tail || node->val != key) return false;
            if (is_marked_ref(succ[0])) return false;
            Node* markedNode = get_marked_ref(node);
            if (!pred[0]->forward[0].compare_exchange_strong(succ[0], markedNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            for (int i = 1; i <= node->topLevel; ++i) {
                while (true) {
                    Node* succNode = node->forward[i].load(std::memory_order_acquire);
                    if (pred[i]->forward[i].compare_exchange_weak(node, succNode,
                            std::memory_order_acq_rel, std::memory_order_acquire))
                        break;
                    Node* curr = pred[i]->forward[i].load(std::memory_order_acquire);
                    if (is_marked_ref(curr)) {
                        Node* unmarked = get_unmarked_ref(curr);
                        pred[i]->forward[i].compare_exchange_weak(curr, unmarked,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                    }
                }
            }
            return true;
        }
    }
};