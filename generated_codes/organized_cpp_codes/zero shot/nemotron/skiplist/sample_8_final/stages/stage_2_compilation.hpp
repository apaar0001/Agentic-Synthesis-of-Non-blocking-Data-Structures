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
        std::atomic<Node*> forward[MAX_LEVEL];

        Node(int v, int level) : val(v), topLevel(level) {
            for (int i = 0; i < MAX_LEVEL; ++i)
                forward[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    Node* head;
    Node* tail;

    int randomLevel() {
        static thread_local std::mt19937 gen{std::random_device{}()};
        static std::uniform_int_distribution<> dist(0, 1);
        int lvl = 0;
        while (dist(gen) && lvl < MAX_LEVEL - 1)
            ++lvl;
        return lvl;
    }

public:
    ConcurrentDataStructure() {
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
                if (is_marked_ref(curr->forward[i].load())) {
                    curr = next;
                    continue;
                }
                if (next == tail || next->val >= key)
                    break;
                curr = next;
            }
        }
        Node* found = get_unmarked_ref(curr->forward[0].load(std::memory_order_acquire));
        return found != tail && found->val == key && !is_marked_ref(curr->forward[0].load());
    }

    bool add(int key) override {
        while (true) {
            Node* update[MAX_LEVEL];
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            Node* pred = head;
            bool valid = true;
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                Node* curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                while (true) {
                    Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                    if (is_marked_ref(curr->forward[i].load())) {
                        valid = false;
                        break;
                    }
                    if (next == tail || next->val >= key) {
                        update[i] = pred;
                        succ[i] = next;
                        break;
                    }
                    pred = next;
                    curr = next;
                }
                if (!valid) break;
            }
            if (!valid) continue;

            Node* found = succ[0];
            if (found != tail && found->val == key && !is_marked_ref(pred->forward[0].load()))
                return false;

            int lvl = randomLevel();
            Node* newNode = new Node(key, lvl);
            for (int i = 0; i <= lvl; ++i)
                newNode->forward[i].store(succ[i], std::memory_order_relaxed);

            for (int i = 0; i <= lvl; ++i) {
                Node* exp = succ[i];
                if (!update[i]->forward[i].compare_exchange_strong(exp, newNode,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    valid = false;
                    break;
                }
            }
            if (!valid) {
                delete newNode;
                continue;
            }
            return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* update[MAX_LEVEL];
            Node* pred[MAX_LEVEL];
            Node* succ[MAX_LEVEL];
            Node* pred = head;
            bool valid = true;
            for (int i = MAX_LEVEL - 1; i >= 0; --i) {
                Node* curr = get_unmarked_ref(pred->forward[i].load(std::memory_order_acquire));
                while (true) {
                    Node* next = get_unmarked_ref(curr->forward[i].load(std::memory_order_acquire));
                    if (is_marked_ref(curr->forward[i].load())) {
                        valid = false;
                        break;
                    }
                    if (next == tail || next->val >= key) {
                        update[i] = pred;
                        succ[i] = next;
                        break;
                    }
                    pred = next;
                    curr = next;
                }
                if (!valid) break;
            }
            if (!valid) continue;

            Node* found = succ[0];
            if (found == tail || found->val != key)
                return false;

            Node* next0 = get_unmarked_ref(found->forward[0].load(std::memory_order_acquire));
            if (!found->forward[0].compare_exchange_strong(next0, get_marked_ref(next0),
                    std::memory_order_acq_rel, std::memory_order_acquire))
                continue;

            for (int i = 0; i < MAX_LEVEL; ++i) {
                if (update[i]->forward[i].load(std::memory_order_acquire) == found) {
                    Node* exp = found;
                    Node* succNode = get_unmarked_ref(found->forward[i].load(std::memory_order_acquire));
                    update[i]->forward[i].compare_exchange_strong(exp, succNode,
                            std::memory_order_acq_rel, std::memory_order_acquire);
                }
            }
            delete found;
            return true;
        }
    }
};