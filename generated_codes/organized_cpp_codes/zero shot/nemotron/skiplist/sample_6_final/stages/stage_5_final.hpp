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

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i) {
                forward[i].store(nullptr, std::memory_order_relaxed);
            }
        }
    };

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static int randomLevel() {
        static thread_local std::mt19937 gen(std::random_device{}());
        static std::bernoulli_distribution dist(0.5);
        int lvl = 0;
        while (dist(gen) && lvl < MAX_LEVEL - 1) ++lvl;
        return lvl;
    }

    Node* head;
    Node* tail;

    void find(int key, Node** update) {
        Node* pred = head;
        for (int i = MAX_LEVEL - 1; i >= 0; --i) {
            Node* curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            while (curr != tail && curr->val < key) {
                pred = curr;
                curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
            }
            update[i] = pred;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, MAX_LEVEL - 1);
        tail = new Node(INT_MAX, MAX_LEVEL - 1);
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
        Node* update[MAX_LEVEL];
        find(key, update);
        Node* found = get_unmarked_ref(update[0]->forward[0].load(std::memory_order_acquire));
        return (found != tail && found->val == key && !is_marked_ref(found->forward[0].load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        retry:
        Node* update[MAX_LEVEL];
        find(key, update);
        Node* found = get_unmarked_ref(update[0]->forward[0].load(std::memory_order_acquire));
        if (found != tail && found->val == key) {
            return false;
        }
        int nodeLevel = randomLevel();
        Node* newNode = new Node(key, nodeLevel);
        for (int i = 0; i <= nodeLevel; ++i) {
            newNode->forward[i].store(get_unmarked_ref(update[i]->forward[i].load(std::memory_order_acquire)),
                                      std::memory_order_relaxed);
        }
        for (int i = 0; i <= nodeLevel; ++i) {
            Node* expected = get_unmarked_ref(update[i]->forward[i].load(std::memory_order_acquire));
            while (!update[i]->forward[i].compare_exchange_weak(expected, newNode,
                                                                std::memory_order_release,
                                                                std::memory_order_relaxed)) {
                expected = get_unmarked_ref(update[i]->forward[i].load(std::memory_order_acquire));
                if (get_unmarked_ref(expected)->val == key) {
                    delete newNode;
                    return false;
                }
            }
        }
        return true;
    }

    bool remove(int key) override {
        Node* update[MAX_LEVEL];
        find(key, update);
        Node* target = get_unmarked_ref(update[0]->forward[0].load(std::memory_order_acquire));
        if (target == tail || target->val != key) {
            return false;
        }
        // Logical deletion: mark next pointer at level 0
        Node* succ = get_unmarked_ref(target->forward[0].load(std::memory_order_acquire));
        while (!is_marked_ref(target->forward[0].load(std::memory_order_acquire))) {
            Node* marked = get_marked_ref(succ);
            if (target->forward[0].compare_exchange_weak(succ, marked,
                                                         std::memory_order_acq_rel,
                                                         std::memory_order_relaxed)) {
                break;
                succ = get_unmarked_ref(target->forward[0].load(std::memory_order_acquire));
            }
        }
        // Physical removal from each level
        for (int i = 0; i <= target->topLevel; ++i) {
            Node* succ_i = get_unmarked_ref(target->forward[i].load(std::memory_order_acquire));
            while (true) {
                Node* pred_next = get_unmarked_ref(update[i]->forward[i].load(std::memory_order_acquire));
                if (pred_next != target) {
                    break; // already removed or helped
                }
                if (update[i]->forward[i].compare_exchange_weak(pred_next, succ_i,
                                                                std::memory_order_release,
                                                                std::memory_order_relaxed)) {
                    break;
                }
            }
        }
        delete target;
        return true;
    }
};