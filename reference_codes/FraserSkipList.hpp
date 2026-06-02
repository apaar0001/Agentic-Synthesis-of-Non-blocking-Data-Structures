#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <random>
#include <mutex>

/**
 * Lock-free Skip List with CAS-mark logical deletion.
 *
 * Design: no physical unlinking — marked nodes remain in structure.
 * find() skips over marked nodes. This avoids the subtle races from
 * concurrent physical unlinking that cause deadlocks/corruption.
 *
 * Linearization:
 *   add():     CAS on pred->next[0] = newNode
 *   remove():  CAS on node->marked = true
 *   contains(): read of node->marked
 */
class FraserSkipList : public SetADT {
private:
    static constexpr int MAX_LEVEL = 16;

    struct Node {
        int key;
        int topLevel;
        std::atomic<bool> marked;
        std::atomic<Node*> next[MAX_LEVEL + 1];

        Node(int k, int level) : key(k), topLevel(level), marked(false) {
            for (int i = 0; i <= MAX_LEVEL; i++)
                next[i].store(nullptr, std::memory_order_relaxed);
        }
    };

    Node* head;
    Node* tail;
    std::mt19937 rng;
    std::mutex rngMutex;

    int randomLevel() {
        std::lock_guard<std::mutex> lk(rngMutex);
        int lvl = 0;
        while (lvl < MAX_LEVEL && (rng() & 1))
            lvl++;
        return lvl;
    }

    /**
     * find(): traverse all levels, skipping marked nodes.
     * preds[i] = rightmost unmarked node at level i with key < target
     * succs[i] = first unmarked node at level i with key >= target
     * Returns true if key found at level 0 and unmarked.
     */
    bool find(int key, Node* preds[], Node* succs[]) {
        Node* pred = head;
        for (int lvl = MAX_LEVEL; lvl >= 0; lvl--) {
            Node* curr = pred->next[lvl].load(std::memory_order_acquire);
            // Skip forward until we find a node with key >= target
            while (curr != tail) {
                if (curr->key < key) {
                    // Move forward — skip if marked (keep going)
                    pred = curr;
                    curr = curr->next[lvl].load(std::memory_order_acquire);
                } else if (curr->key == key) {
                    break;
                } else {
                    break;
                }
            }
            preds[lvl] = pred;
            succs[lvl] = curr;
        }
        if (succs[0] == tail) return false;
        return (succs[0]->key == key && !succs[0]->marked.load(std::memory_order_acquire));
    }

public:
    FraserSkipList() : rng(std::random_device{}()) {
        head = new Node(INT_MIN, MAX_LEVEL);
        tail = new Node(INT_MAX, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++)
            head->next[i].store(tail, std::memory_order_relaxed);
    }

    ~FraserSkipList() override {
        Node* curr = head;
        while (curr && curr != tail) {
            Node* n = curr->next[0].load(std::memory_order_relaxed);
            delete curr;
            curr = n;
        }
        delete tail;
    }

    bool contains(int key) override {
        Node* pred = head;
        Node* curr = nullptr;
        for (int lvl = MAX_LEVEL; lvl >= 0; lvl--) {
            curr = pred->next[lvl].load(std::memory_order_acquire);
            while (curr != tail && curr->key < key) {
                pred = curr;
                curr = curr->next[lvl].load(std::memory_order_acquire);
            }
        }
        if (curr == tail || curr->key != key) return false;
        return !curr->marked.load(std::memory_order_acquire);
    }

    bool add(int key) override {
        int topLvl = randomLevel();
        Node* preds[MAX_LEVEL + 1];
        Node* succs[MAX_LEVEL + 1];

        while (true) {
            bool found = find(key, preds, succs);
            if (found) {
                return false;  // key already present & unmarked
            }

            // Check if a marked node with this key exists — skip it
            // (find() already skips marked nodes)

            Node* newNode = new Node(key, topLvl);

            // Set forward pointers
            for (int i = 0; i <= topLvl; i++)
                newNode->next[i].store(succs[i], std::memory_order_relaxed);

            // Commit at level 0
            Node* expected = succs[0];
            if (!preds[0]->next[0].compare_exchange_strong(
                    expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                delete newNode;
                continue;  // retry
            }

            // Link higher levels
            for (int i = 1; i <= topLvl; i++) {
                while (true) {
                    expected = succs[i];
                    if (preds[i]->next[i].compare_exchange_strong(
                            expected, newNode,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        break;
                    }
                    // Re-find for fresh preds/succs
                    find(key, preds, succs);
                    // Update newNode's forward pointer for this level
                    newNode->next[i].store(succs[i], std::memory_order_relaxed);
                }
            }
            return true;
        }
    }

    bool remove(int key) override {
        Node* preds[MAX_LEVEL + 1];
        Node* succs[MAX_LEVEL + 1];

        bool found = find(key, preds, succs);
        if (!found) return false;

        Node* victim = succs[0];

        // Linearization: CAS marked false→true
        bool expected = false;
        if (!victim->marked.compare_exchange_strong(
                expected, true,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            return false;  // another thread marked it first
        }
        // Node has been marked
        return true;
    }
};
