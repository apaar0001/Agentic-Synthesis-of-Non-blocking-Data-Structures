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

    std::atomic<Node*> head;

    struct SearchResult {
        std::atomic<Node*>* parentPtr;
        bool isLeftChild;
        Node* node;
    };

    SearchResult search(int key) {
        Node* headNode = head.load(std::memory_order_acquire);
        std::atomic<Node*>* parentPtr = &headNode->left;
        bool isLeft = true;
        Node* cur = parentPtr->load(std::memory_order_acquire);
        while (true) {
            if (!cur) {
                return {parentPtr, isLeft, nullptr};
            }
            Node* unmarked = get_unmarked_ref(cur);
            if (is_marked_ref(cur)) {
                helpDelete(parentPtr, isLeft, cur);
                cur = parentPtr->load(std::memory_order_acquire);
                continue;
            }
            if (unmarked->val == key) {
                return {parentPtr, isLeft, unmarked};
            }
            bool goLeft = key < unmarked->val;
            Node* next = goLeft ? unmarked->left.load(std::memory_order_acquire)
                                : unmarked->right.load(std::memory_order_acquire);
            parentPtr = goLeft ? &unmarked->left : &unmarked->right;
            isLeft = goLeft;
            cur = next;
        }
    }

    bool helpDelete(std::atomic<Node*>* parentPtr, bool isLeftChild, Node* victim) {
        Node* unmarkedVictim = get_unmarked_ref(victim);
        Node* left = unmarkedVictim->left.load(std::memory_order_acquire);
        Node* right = unmarkedVictim->right.load(std::memory_order_acquire);
        Node* replacement = nullptr;
        Node* uleft = get_unmarked_ref(left);
        Node* uright = get_unmarked_ref(right);
        if (uleft && !is_marked_ref(left)) {
            replacement = uleft;
        } else if (uright && !is_marked_ref(right)) {
            replacement = uright;
        }
        Node* expected = get_marked_ref(victim);
        Node* desired = get_unmarked_ref(replacement);
        return parentPtr->compare_exchange_strong(expected, desired,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_acquire);
    }

    std::tuple<std::atomic<Node*>*, bool, Node*> findMin(Node* start,
                                                        std::atomic<Node*>* parentPtr,
                                                        bool isLeftChild) {
        Node* cur = start;
        std::atomic<Node*>* pp = parentPtr;
        bool isLeft = isLeftChild;
        while (true) {
            Node* left = cur->left.load(std::memory_order_acquire);
            Node* uleft = get_unmarked_ref(left);
            if (uleft && !is_marked_ref(left)) {
                pp = &cur->left;
                isLeft = true;
                cur = uleft;
                continue;
            }
            break;
        }
        return {pp, isLeft, cur};
    }

    std::tuple<std::atomic<Node*>*, bool, Node*> findMax(Node* start,
                                                        std::atomic<Node*>* parentPtr,
                                                        bool isLeftChild) {
        Node* cur = start;
        std::atomic<Node*>* pp = parentPtr;
        bool isLeft = isLeftChild;
        while (true) {
            Node* right = cur->right.load(std::memory_order_acquire);
            Node* uright = get_unmarked_ref(right);
            if (uright && !is_marked_ref(right)) {
                pp = &cur->right;
                isLeft = false;
                cur = uright;
                continue;
            }
            break;
        }
        return {pp, isLeft, cur};
    }

public:
    ConcurrentDataStructure() {
        Node* sentinelMin = new Node(INT_MIN);
        Node* sentinelMax = new Node(INT_MAX);
        Node* dummy = new Node(0);
        dummy->left.store(sentinelMin, std::memory_order_relaxed);
        dummy->right.store(sentinelMax, std::memory_order_relaxed);
        head.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* start = head.load(std::memory_order_acquire);
        std::vector<Node*> stack;
        stack.push_back(start);
        while (!stack.empty()) {
            Node* n = stack.back();
            stack.pop_back();
            Node* unmarked = get_unmarked_ref(n);
            if (!unmarked) continue;
            Node* left = unmarked->left.load(std::memory_order_acquire);
            Node* right = unmarked->right.load(std::memory_order_acquire);
            if (left) stack.push_back(get_unmarked_ref(left));
            if (right) stack.push_back(get_unmarked_ref(right));
            delete unmarked;
        }
    }

    bool contains(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (!res.node) {
                return false;
            }
            if (is_marked_ref(res.node)) {
                helpDelete(res.parentPtr, res.isLeftChild, res.node);
                continue;
            }
            return true;
        }
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (res.node) {
                if (!is_marked_ref(res.node)) {
                    return false;
                }
                helpDelete(res.parentPtr, res.isLeftChild, res.node);
                continue;
            }
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (res.parentPtr->compare_exchange_strong(expected, newNode,
                                                      std::memory_order_release,
                                                      std::memory_order_relaxed)) {
                                                          std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (!res.node) {
                return false;
            }
            if (is_marked_ref(res.node)) {
                helpDelete(res.parentPtr, res.isLeftChild, res.node);
                continue;
            }
            Node* marked = get_marked_ref(res.node);
            Node* expected = res.node;
            if (!res.parentPtr->compare_exchange_strong(expected, marked,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
                continue;
            }
            // Attempt to physically remove the marked node
            Node* rightChild = get_unmarked_ref(res.node->right.load(std::memory_order_acquire));
            Node* leftChild = get_unmarked_ref(res.node->left.load(std::memory_order_acquire));
            Node* succ = nullptr;
            std::atomic<Node*>* succParentPtr = nullptr;
            bool succIsLeft = false;

            if (rightChild) {
                auto [pp, isLeft, minNode] = findMin(rightChild, &res.node->right, false);
                succParentPtr = pp;
                succIsLeft = isLeft;
                succ = minNode;
            } else if (leftChild) {
                auto [pp, isLeft, maxNode] = findMax(leftChild, &res.node->left, true);
                succParentPtr = pp;
                succIsLeft = isLeft;
                succ = maxNode;
            }

            if (succ) {
                // Copy successor's value into the node to be deleted
                res.node->val = succ->val;
                // Mark successor
                Node* succMarked = get_marked_ref(succ);
                Node* succExpected = succ;
                if (!succParentPtr->compare_exchange_strong(succExpected, succMarked,
                                                           std::memory_order_acq_rel,
                                                           std::memory_order_acquire)) {
                    // If failed, retry whole remove operation
                    continue;
                }
                // Help unlink successor
                helpDelete(succParentPtr, succIsLeft, succ);
            } else {
                // Node has no children, just help unlink it
                helpDelete(res.parentPtr, res.isLeftChild, res.node);
            }
            return true;
        }
    }
};