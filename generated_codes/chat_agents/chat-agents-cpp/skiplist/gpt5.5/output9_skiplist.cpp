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

    int randomLevel() {
        thread_local std::mt19937 gen(std::random_device{}());
        int level = 0;
        while (level < MAX_LEVEL - 1 && (gen() & 1)) {
            ++level;
        }
        return level;
    }

    bool find(int key, Node* preds[], Node* succs[]) {
    retry:
        Node* pred = head;

        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            Node* curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));

            while (true) {
                Node* succ = curr->forward[level].load(std::memory_order_acquire);

                while (is_marked_ref(succ)) {
                    Node* unmarkedSucc = get_unmarked_ref(succ);
                    Node* expected = curr;

                    if (!pred->forward[level].compare_exchange_strong(
                            expected,
                            unmarkedSucc,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        goto retry;
                    }

                    curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));
                    succ = curr->forward[level].load(std::memory_order_acquire);
                }

                if (curr->val < key) {
                    pred = curr;
                    curr = get_unmarked_ref(succ);
                } else {
                    break;
                }
            }

            preds[level] = pred;
            succs[level] = curr;
        }

        return succs[0]->val == key;
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

    ~ConcurrentDataStructure() override {
        Node* curr = head;

        while (curr) {
            Node* next = nullptr;

            if (curr != tail) {
                next = get_unmarked_ref(
                    curr->forward[0].load(std::memory_order_relaxed));
            }

            delete curr;

            if (curr == tail) {
                break;
            }

            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;

        for (int level = MAX_LEVEL - 1; level >= 0; --level) {
            curr = get_unmarked_ref(pred->forward[level].load(std::memory_order_acquire));

            while (curr->val < key) {
                pred = curr;
                curr = get_unmarked_ref(curr->forward[level].load(std::memory_order_acquire));
            }
        }

        Node* succ = curr->forward[0].load(std::memory_order_acquire);

        return curr->val == key && !is_marked_ref(succ);
    }

    bool add(int key) override {
        int topLevel = randomLevel();
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];

        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }

            Node* newNode = new Node(key, topLevel);

            for (int level = 0; level <= topLevel; ++level) {
                newNode->forward[level].store(
                    succs[level],
                    std::memory_order_relaxed);
            }

            Node* expected = succs[0];

            if (!preds[0]->forward[0].compare_exchange_strong(
                    expected,
                    newNode,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                delete newNode;
                continue;
            }

            for (int level = 1; level <= topLevel; ++level) {
                while (true) {
                    expected = succs[level];

                    if (preds[level]->forward[level].compare_exchange_strong(
                            expected,
                            newNode,
                            std::memory_order_acq_rel,
                            std::memory_order_acquire)) {
                        break;
                    }

                    find(key, preds, succs);
                }
            }

            return true;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL];
        Node* succs[MAX_LEVEL];
        Node* nodeToRemove = nullptr;
        bool isMarked = false;

        while (true) {
            bool found = find(key, preds, succs);

            if (!found) {
                return false;
            }

            nodeToRemove = succs[0];

            for (int level = nodeToRemove->topLevel; level >= 1; --level) {
                Node* succ = nodeToRemove->forward[level].load(std::memory_order_acquire);

                while (!is_marked_ref(succ)) {
                    Node* marked = get_marked_ref(get_unmarked_ref(succ));

                    nodeToRemove->forward[level].compare_exchange_strong(
                        succ,
                        marked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire);

                    succ = nodeToRemove->forward[level].load(std::memory_order_acquire);
                }
            }

            Node* succ = nodeToRemove->forward[0].load(std::memory_order_acquire);

            while (true) {
                if (is_marked_ref(succ)) {
                    isMarked = true;
                    break;
                }

                Node* marked = get_marked_ref(get_unmarked_ref(succ));

                if (nodeToRemove->forward[0].compare_exchange_strong(
                        succ,
                        marked,
                        std::memory_order_acq_rel,
                        std::memory_order_acquire)) {
                    isMarked = false;
                    break;
                }
            }

            if (isMarked) {
                return false;
            }

            find(key, preds, succs);
            return true;
        }
    }
};
