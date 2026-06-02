#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    std::atomic<Node*> root{nullptr};

    // Returns pointer to the atomic reference that holds the node (if found) or the insertion point (if not found).
    // Sets 'found' to true if a node with the key exists (regardless of mark).
    std::atomic<Node*>* findRef(int key, bool& found) {
        std::atomic<Node*>* currRef = &root;
        while (true) {
            Node* curr = currRef->load(std::memory_order_acquire);
            if (!curr) {
                found = false;
                return currRef;
            }
            if (is_marked_ref(curr)) {
                // Help unlink this marked node from its parent (currRef)
                Node* unmarked = get_unmarked_ref(curr);
                Node* replacement = unmarked->left.load(std::memory_order_acquire);
                if (!replacement) {
                    replacement = unmarked->right.load(std::memory_order_acquire);
                }
                Node* expected = curr;
                if (currRef->compare_exchange_strong(expected, replacement,
                                                   std::memory_order_acq_rel, std::memory_order_acquire)) {
                    // After successful help, retry without advancing
                    continue;
                }
                // CAS failed, another thread updated currRef; retry from start of loop
                continue;
            }
            // curr is unmarked
            if (key < curr->val) {
                currRef = &curr->left;
            } else if (key > curr->val) {
                currRef = &curr->right;
            } else {
                found = true;
                return currRef;
            }
        }
    }

    static void destroy(Node* n) {
        if (!n) return;
        destroy(get_unmarked_ref(n->left.load(std::memory_order_relaxed)));
        destroy(get_unmarked_ref(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

public:
    ConcurrentDataStructure() = default;
    ~ConcurrentDataStructure() override {
        destroy(get_unmarked_ref(root.load(std::memory_order_relaxed)));
    }

    bool contains(int key) override {
        bool found;
        std::atomic<Node*>* ref = findRef(key, found);
        if (!found) return false;
        Node* node = ref->load(std::memory_order_acquire);
        return !is_marked_ref(node);
    }

    bool add(int key) override {
        while (true) {
            bool found;
            std::atomic<Node*>* ref = findRef(key, found);
            if (found) {
                Node* node = ref->load(std::memory_order_acquire);
                if (is_marked_ref(node)) {
                    // Node is marked (logically deleted), treat as absent and retry
                    continue;
                }
                return false; // duplicate
            }
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (ref->compare_exchange_strong(expected, newNode,
                                             std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            // CAS failed; expected holds current value (maybe a node). Loop to retry.
        }
    }

    bool remove(int key) override {
        while (true) {
            bool found;
            std::atomic<Node*>* ref = findRef(key, found);
            if (!found) return false;
            Node* curr = ref->load(std::memory_order_acquire);
            if (is_marked_ref(curr)) {
                // Node already marked by another thread
                return false;
            }
            Node* marked = get_marked_ref(curr);
            if (ref->compare_exchange_strong(curr, marked,
                                             std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
            // CAS failed, retry
        }
    }
};