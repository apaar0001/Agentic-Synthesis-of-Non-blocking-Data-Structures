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

    std::atomic<Node*> root;

    static Node* get_marked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static Node* get_unmarked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static bool is_marked(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    void helpDelete(Node* parent, bool isLeft, Node* marked) {
        Node* child = isLeft ? marked->left.load(std::memory_order_acquire)
                             : marked->right.load(std::memory_order_acquire);
        Node* unmarkedChild = get_unmarked(child);
        std::atomic<Node*>& parentPtr = isLeft ? parent->left : parent->right;
        Node* expected = get_marked(marked);
        parentPtr.compare_exchange_strong(expected, unmarkedChild,
                                          std::memory_order_acq_rel, std::memory_order_acquire);
    }

public:
    ConcurrentDataStructure() : root(nullptr) {}
    ~ConcurrentDataStructure() = default;

    bool contains(int key) override {
        while (true) {
            Node* prev = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            bool fromLeft = false;
            while (curr) {
                if (is_marked(curr)) {
                    if (prev) helpDelete(prev, fromLeft, curr);
                    break;
                }
                Node* u = get_unmarked(curr);
                if (key == u->val) {
                    return !is_marked(curr);
                }
                prev = u;
                if (key < u->val) {
                    curr = u->left.load(std::memory_order_acquire);
                    fromLeft = true;
                } else {
                    curr = u->right.load(std::memory_order_acquire);
                    fromLeft = false;
                }
            }
            if (!curr || !is_marked(curr)) {
                return false;
            }
        }
    }

    bool add(int key) override {
        while (true) {
            Node* prev = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            bool fromLeft = false;
            while (curr) {
                if (is_marked(curr)) {
                    if (prev) helpDelete(prev, fromLeft, curr);
                    break;
                }
                Node* u = get_unmarked(curr);
                if (key == u->val) {
                    return false;
                }
                prev = u;
                if (key < u->val) {
                    curr = u->left.load(std::memory_order_acquire);
                    fromLeft = true;
                } else {
                    curr = u->right.load(std::memory_order_acquire);
                    fromLeft = false;
                }
            }
            if (is_marked(curr)) {
                continue;
            }
            Node* newNode = new Node(key);
            if (!prev) {
                Node* expected = nullptr;
                if (root.compare_exchange_strong(expected, newNode,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    return true;
                }
            } else {
                std::atomic<Node*>& childPtr = fromLeft ? prev->left : prev->right;
                Node* expected = nullptr;
                if (childPtr.compare_exchange_strong(expected, newNode,
                        std::memory_order_release, std::memory_order_relaxed)) {
                    return true;
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* prev = nullptr;
            Node* curr = root.load(std::memory_order_acquire);
            bool isLeftChild = false;
            bool found = false;
            while (curr) {
                if (is_marked(curr)) {
                    if (prev) helpDelete(prev, isLeftChild, curr);
                    break;
                }
                Node* u = get_unmarked(curr);
                if (key == u->val) {
                    found = true;
                    break;
                }
                prev = u;
                if (key < u->val) {
                    curr = u->left.load(std::memory_order_acquire);
                    isLeftChild = true;
                } else {
                    curr = u->right.load(std::memory_order_acquire);
                    isLeftChild = false;
                }
            }
            if (!found) {
                return false;
            }
            std::atomic<Node*>& parentPtr = prev ?
                (isLeftChild ? prev->left : prev->right) : root;
            Node* expected = curr;
            Node* desired = get_marked(curr);
            if (parentPtr.compare_exchange_strong(expected, desired,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};