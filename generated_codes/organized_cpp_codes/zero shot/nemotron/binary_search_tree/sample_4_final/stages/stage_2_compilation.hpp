#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
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

    std::atomic<Node*> head_;

    // Returns true if node with key found (and not marked), and sets parentPtr and isLeftChild.
    bool find(int key, std::atomic<Node*>*& parentPtr, Node*& nodePtr, bool& isLeftChild) const {
        parentPtr = nullptr;
        nodePtr = head_.load(std::memory_order_acquire);
        isLeftChild = false;
        while (nodePtr) {
            int v = nodePtr->val;
            if (key < v) {
                parentPtr = &nodePtr->left;
                Node* next = nodePtr->left.load(std::memory_order_acquire);
                // skip marked nodes
                while (next && is_marked_ref(next)) {
                    next = nullptr; // treat as absent
                    break;
                }
                if (!next) {
                    nodePtr = nullptr;
                    break;
                }
                nodePtr = next;
                isLeftChild = true;
            } else if (key > v) {
                parentPtr = &nodePtr->right;
                Node* next = nodePtr->right.load(std::memory_order_acquire);
                while (next && is_marked_ref(next)) {
                    next = nullptr;
                    break;
                }
                if (!next) {
                    nodePtr = nullptr;
                    break;
                }
                nodePtr = next;
                isLeftChild = false;
            } else {
                // key equal
                if (is_marked_ref(nodePtr)) {
                    nodePtr = nullptr;
                }
                break;
            }
        }
        return nodePtr != nullptr && !is_marked_ref(nodePtr);
    }

    void deleteSubtree(Node* node) {
        if (!node) return;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_relaxed));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
        deleteSubtree(left);
        deleteSubtree(right);
        delete node;
    }

public:
    ConcurrentDataStructure() {
        Node* dummy = new Node(0);
        Node* leftSentinel = new Node(INT_MIN);
        Node* rightSentinel = new Node(INT_MAX);
        dummy->left.store(leftSentinel, std::memory_order_relaxed);
        dummy->right.store(rightSentinel, std::memory_order_relaxed);
        head_.store(dummy, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* dummy = head_.load(std::memory_order_acquire);
        Node* left = get_unmarked_ref(dummy->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(dummy->right.load(std::memory_order_acquire));
        deleteSubtree(left);
        deleteSubtree(right);
        delete dummy;
    }

    bool contains(int key) override {
        std::atomic<Node*>* parentPtr;
        Node* nodePtr;
        bool isLeftChild;
        return find(key, parentPtr, nodePtr, isLeftChild);
    }

    bool add(int key) override {
        while (true) {
            std::atomic<Node*>* parentPtr;
            Node* nodePtr;
            bool isLeftChild;
            if (find(key, parentPtr, nodePtr, isLeftChild)) {
                return false; // already present
            }
            Node* newNode = new Node(key);
            newNode->left.store(nullptr, std::memory_order_relaxed);
            newNode->right.store(nullptr, std::memory_order_relaxed);
            Node* expected = nullptr;
            if (parentPtr->compare_exchange_strong(
                    expected, newNode,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                return true;
            }
            // CAS failed, retry
            delete newNode; // avoid leak if we allocated but didn't link
        }
    }

    bool remove(int key) override {
        while (true) {
            std::atomic<Node*>* parentPtr;
            Node* nodePtr;
            bool isLeftChild;
            if (!find(key, parentPtr, nodePtr, isLeftChild)) {
                return false; // not present
            }
            // Logically mark the node
            Node* expected = nodePtr;
            Node* marked = get_marked_ref(nodePtr);
            if (!parentPtr->compare_exchange_strong(
                    expected, marked,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                // CAS failed, retry
                continue;
            }
            // Physically unlink: replace with non-null child if any, preferring left
            Node* leftChild = get_unmarked_ref(nodePtr->left.load(std::memory_order_acquire));
            Node* rightChild = get_unmarked_ref(nodePtr->right.load(std::memory_order_acquire));
            Node* replacement = nullptr;
            if (leftChild) {
                replacement = leftChild;
            } else if (rightChild) {
                replacement = rightChild;
            }
            // Attempt to link replacement in place of the marked node
            Node* exp = marked;
            if (parentPtr->compare_exchange_strong(
                    exp, replacement,
                    std::memory_order_release,
                    std::memory_order_relaxed)) {
                delete nodePtr;
                return true;
            }
            // If CAS failed, retry whole operation (the node may have been changed)
        }
    }
};