#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstdint>
#include <climits>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    std::atomic<Node*> root;

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    bool helpDelete(Node* parent, std::atomic<Node*>* childPtr, Node* victim) {
        Node* left = get_unmarked_ref(victim->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(victim->right.load(std::memory_order_acquire));
        Node* replacement = left ? left : right;
        Node* expected = get_marked_ref(victim);
        return childPtr->compare_exchange_strong(expected, replacement,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
    }

public:
    ConcurrentDataStructure() : root(nullptr) {}
    ~ConcurrentDataStructure() {}

    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr) {
            if (is_marked_ref(curr)) {
                curr = root.load(std::memory_order_acquire);
                continue;
            }
            Node* unmarked = get_unmarked_ref(curr);
            if (key == unmarked->val) {
                return true;
            }
            if (key < unmarked->val) {
                curr = get_unmarked_ref(unmarked->left.load(std::memory_order_acquire));
            } else {
                curr = get_unmarked_ref(unmarked->right.load(std::memory_order_acquire));
            }
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* childPtr = nullptr;

            while (true) {
                Node* currUnmarked = get_unmarked_ref(curr);
                if (!currUnmarked) {
                    break;
                }
                if (is_marked_ref(curr)) {
                    if (parent) {
                        std::atomic<Node*>* ptr = (key < parent->val ? &parent->left : &parent->right);
                        helpDelete(parent, ptr, get_unmarked_ref(curr));
                    } else {
                        helpDelete(nullptr, &root, get_unmarked_ref(curr));
                    }
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    childPtr = nullptr;
                    continue;
                }
                if (key == currUnmarked->val) {
                    return false;
                }
                parent = currUnmarked;
                if (key < currUnmarked->val) {
                    childPtr = &parent->left;
                } else {
                    childPtr = &parent->right;
                }
                Node* nextRaw = childPtr->load(std::memory_order_acquire);
                curr = get_unmarked_ref(nextRaw);
            }

            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (childPtr->compare_exchange_strong(expected, newNode,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* curr = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* childPtr = nullptr;
            Node* target = nullptr;

            while (true) {
                Node* currUnmarked = get_unmarked_ref(curr);
                if (!currUnmarked) {
                    return false;
                }
                if (is_marked_ref(curr)) {
                    if (parent) {
                        std::atomic<Node*>* ptr = (key < parent->val ? &parent->left : &parent->right);
                        helpDelete(parent, ptr, get_unmarked_ref(curr));
                    } else {
                        helpDelete(nullptr, &root, get_unmarked_ref(curr));
                    }
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    childPtr = nullptr;
                    continue;
                }
                if (key == currUnmarked->val) {
                    target = currUnmarked;
                    break;
                }
                parent = currUnmarked;
                if (key < currUnmarked->val) {
                    childPtr = &parent->left;
                } else {
                    childPtr = &parent->right;
                }
                Node* nextRaw = childPtr->load(std::memory_order_acquire);
                curr = get_unmarked_ref(nextRaw);
            }

            if (is_marked_ref(target)) {
                return false;
            }

            Node* expected = get_unmarked_ref(target);
            Node* desired = get_marked_ref(target);
            if (childPtr->compare_exchange_strong(expected, desired,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};