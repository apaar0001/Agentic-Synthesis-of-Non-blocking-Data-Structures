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
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static inline Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static inline Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static inline bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    Node* head; // dummy sentinel, head->left points to the tree root

    void help_unlink(std::atomic<Node*>* parentPtr, Node* markedNode) {
        Node* left = get_unmarked_ref(markedNode->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(markedNode->right.load(std::memory_order_acquire));
        Node* replacement = left ? left : right;
        Node* expected = get_marked_ref(markedNode);
        parentPtr->compare_exchange_strong(expected, replacement,
                                           std::memory_order_acq_rel, std::memory_order_acquire);
    }

public:
    ConcurrentDataStructure() {
        head = new Node(0);
        head->left.store(nullptr, std::memory_order_relaxed);
        head->right.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        // Leak nodes as allowed; only delete sentinel
        delete head;
    }

    bool contains(int key) override {
        while (true) {
            Node* parent = head;
            bool goLeft = false;
            while (true) {
                std::atomic<Node*>* childPtr = goLeft ? &(parent->left) : &(parent->right);
                Node* raw = childPtr->load(std::memory_order_acquire);
                Node* child = get_unmarked_ref(raw);
                if (is_marked_ref(raw)) {
                    // help unlink marked node
                    help_unlink(childPtr, child);
                    break; // restart outer loop
                }
                if (!child) {
                    return false;
                }
                if (child->val == key) {
                    return true;
                }
                goLeft = key < child->val;
                parent = child;
            }
        }
    }

    bool add(int key) override {
        while (true) {
            Node* parent = head;
            bool goLeft = false;
            while (true) {
                std::atomic<Node*>* childPtr = goLeft ? &(parent->left) : &(parent->right);
                Node* raw = childPtr->load(std::memory_order_acquire);
                Node* child = get_unmarked_ref(raw);
                if (is_marked_ref(raw)) {
                    // help unlink marked node
                    help_unlink(childPtr, child);
                    break;
                }
                if (!child) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (childPtr->compare_exchange_strong(expected, newNode,
                                                          std::memory_order_acq_rel, std::memory_order_acquire)) {
                        return true;
                    } else {
                        break;
                    }
                }
                if (child->val == key) {
                    return false;
                }
                goLeft = key < child->val;
                parent = child;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent = head;
            bool goLeft = false;
            Node* target = nullptr;
            std::atomic<Node*>* targetPtr = nullptr;
            bool found = false;
            while (true) {
                std::atomic<Node*>* childPtr = goLeft ? &(parent->left) : &(parent->right);
                Node* raw = childPtr->load(std::memory_order_acquire);
                Node* child = get_unmarked_ref(raw);
                if (is_marked_ref(raw)) {
                    // help unlink marked node
                    help_unlink(childPtr, child);
                    break;
                }
                if (!child) {
                    return false;
                }
                if (child->val == key) {
                    target = child;
                    targetPtr = childPtr;
                    found = true;
                    break;
                }
                goLeft = key < child->val;
                parent = child;
            }
            if (!found) {
                return false;
            }
            Node* expected = targetPtr->load(std::memory_order_acquire);
            if (is_marked_ref(expected) || get_unmarked_ref(expected) != target) {
                // changed or already marked, retry
                continue;
            }
            Node* marked = get_marked_ref(expected);
            if (!targetPtr->compare_exchange_strong(expected, marked,
                                                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            // Now physically unlink the marked node
            help_unlink(targetPtr, target);
            return true;
        }
    }
};