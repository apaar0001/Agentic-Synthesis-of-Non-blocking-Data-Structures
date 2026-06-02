#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* root;        // dummy node
    Node* minSentinel; // holds INT_MIN
    Node* maxSentinel; // holds INT_MAX

    void helpRemoveParent(Node* parent, bool isLeft, Node* markedNode) {
        Node* leftChild = get_unmarked_ref(markedNode->left.load(std::memory_order_acquire));
        Node* rightChild = get_unmarked_ref(markedNode->right.load(std::memory_order_acquire));
        Node* replacement = nullptr;
        if (!leftChild) replacement = rightChild;
        else if (!rightChild) replacement = leftChild;
        else {
            // two children: leave as is (lazy deletion)
            replacement = leftChild; // arbitrary, will not break ordering if later removed
        }
        Node* expected = get_marked_ref(markedNode);
        if (isLeft) {
            parent->left.compare_exchange_strong(expected, replacement,
                std::memory_order_acq_rel, std::memory_order_acquire);
        } else {
            parent->right.compare_exchange_strong(expected, replacement,
                std::memory_order_acq_rel, std::memory_order_acquire);
        }
    }

public:
    ConcurrentDataStructure() {
        root = new Node(0);
        minSentinel = new Node(INT_MIN);
        maxSentinel = new Node(INT_MAX);
        root->left.store(minSentinel, std::memory_order_release);
        root->right.store(maxSentinel, std::memory_order_release);
        minSentinel->left.store(nullptr, std::memory_order_relaxed);
        minSentinel->right.store(nullptr, std::memory_order_relaxed);
        maxSentinel->left.store(nullptr, std::memory_order_relaxed);
        maxSentinel->right.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        std::vector<Node*> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            Node* n = stack.back(); stack.pop_back();
            if (!n) continue;
            Node* l = get_unmarked_ref(n->left.load(std::memory_order_relaxed));
            Node* r = get_unmarked_ref(n->right.load(std::memory_order_relaxed));
            stack.push_back(l);
            stack.push_back(r);
            delete n;
        }
    }

    bool contains(int key) override {
        Node* curr = root;
        while (true) {
            if (key < curr->val) {
                Node* child = curr->left.load(std::memory_order_acquire);
                if (!child) return false;
                if (is_marked_ref(child)) {
                    helpRemoveParent(curr, false, get_unmarked_ref(child));
                    child = curr->left.load(std::memory_order_acquire);
                    if (!child) return false;
                    curr = get_unmarked_ref(child);
                    continue;
                }
                curr = get_unmarked_ref(child);
            } else if (key > curr->val) {
                Node* child = curr->right.load(std::memory_order_acquire);
                if (!child) return false;
                if (is_marked_ref(child)) {
                    helpRemoveParent(curr, true, get_unmarked_ref(child));
                    child = curr->right.load(std::memory_order_acquire);
                    if (!child) return false;
                    curr = get_unmarked_ref(child);
                    continue;
                }
                curr = get_unmarked_ref(child);
            } else {
                if (curr->val == INT_MIN || curr->val == INT_MAX) return false;
                return true;
            }
        }
    }

    bool add(int key) override {
        while (true) {
            Node* parent = root;
            while (true) {
                if (key < parent->val) {
                    Node* child = parent->left.load(std::memory_order_acquire);
                    if (!child) {
                        Node* newNode = new Node(key);
                        Node* expected = nullptr;
                        if (parent->left.compare_exchange_strong(expected, newNode,
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                            return true;
                        }
                    } else {
                        if (is_marked_ref(child)) {
                            helpRemoveParent(parent, false, get_unmarked_ref(child));
                            continue;
                        }
                        parent = get_unmarked_ref(child);
                    }
                } else if (key > parent->val) {
                    Node* child = parent->right.load(std::memory_order_acquire);
                    if (!child) {
                        Node* newNode = new Node(key);
                        Node* expected = nullptr;
                        if (parent->right.compare_exchange_strong(expected, newNode,
                                std::memory_order_acq_rel, std::memory_order_acquire)) {
                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                            return true;
                        }
                    } else {
                        if (is_marked_ref(child)) {
                            helpRemoveParent(parent, true, get_unmarked_ref(child));
                            continue;
                        }
                        parent = get_unmarked_ref(child);
                    }
                } else {
                    return false;
                }
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent = root;
            bool isLeft = false;
            Node* curr = nullptr;
            while (true) {
                if (key < parent->val) {
                    curr = parent->left.load(std::memory_order_acquire);
                    isLeft = false;
                } else if (key > parent->val) {
                    curr = parent->right.load(std::memory_order_acquire);
                    isLeft = true;
                } else {
                    break;
                }
                if (!curr) return false;
                if (is_marked_ref(curr)) {
                    helpRemoveParent(parent, isLeft, get_unmarked_ref(curr));
                    break;
                }
                parent = get_unmarked_ref(curr);
            }
            if (!curr) return false;
            if (is_marked_ref(curr)) {
                helpRemoveParent(parent, isLeft, get_unmarked_ref(curr));
                return true;
            }
            Node* marked = get_marked_ref(curr);
            if (isLeft) {
                if (!parent->left.compare_exchange_strong(curr, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }
            } else {
                if (!parent->right.compare_exchange_strong(curr, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    continue;
                }
            }
            Node* leftChild = get_unmarked_ref(curr->left.load(std::memory_order_acquire));
            Node* rightChild = get_unmarked_ref(curr->right.load(std::memory_order_acquire));
            Node* replacement = nullptr;
            if (!leftChild) replacement = rightChild;
            else if (!rightChild) replacement = leftChild;
            else {
                replacement = leftChild; // lazy, will be cleaned later
            }
            Node* expected = marked;
            if (isLeft) {
                parent->left.compare_exchange_strong(expected, replacement,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            } else {
                parent->right.compare_exchange_strong(expected, replacement,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            }
            return true;
        }
    }
};