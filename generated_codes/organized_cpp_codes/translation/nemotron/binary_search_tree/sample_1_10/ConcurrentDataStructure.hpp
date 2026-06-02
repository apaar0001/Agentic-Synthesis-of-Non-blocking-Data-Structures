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

    static inline Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static inline Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static inline bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1)) != 0;
    }

    static void helpUnlink(Node* parent, bool isLeftChild) {
        std::atomic<Node*>& link = isLeftChild ? parent->left : parent->right;
        Node* markedPtr = link.load(std::memory_order_acquire);
        if (!is_marked_ref(markedPtr)) {
            return;
        }
        Node* node = get_unmarked_ref(markedPtr);
        Node* leftChild = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* rightChild = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        Node* replacement = leftChild ? leftChild : rightChild;
        link.compare_exchange_strong(markedPtr, replacement,
                                     std::memory_order_release, std::memory_order_relaxed);
    }

    void clear(Node* node) {
        if (!node) return;
        clear(get_unmarked_ref(node->left.load(std::memory_order_acquire)));
        clear(get_unmarked_ref(node->right.load(std::memory_order_acquire)));
        delete node;
    }

public:
    ConcurrentDataStructure() : root(nullptr) {}
    ~ConcurrentDataStructure() override {
        clear(get_unmarked_ref(root.load(std::memory_order_acquire)));
    }

    bool contains(int key) override {
        Node* curr = get_unmarked_ref(root.load(std::memory_order_acquire));
        while (curr) {
            if (key == curr->val) {
                return true;
            }
            bool goLeft = key < curr->val;
            Node* next = goLeft ? curr->left.load(std::memory_order_acquire)
                                : curr->right.load(std::memory_order_acquire);
            if (is_marked_ref(next)) {
                helpUnlink(curr, goLeft);
                return false;
            }
            curr = get_unmarked_ref(next);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = get_unmarked_ref(root.load(std::memory_order_acquire));
            bool goLeft = false;
            while (curr) {
                if (key == curr->val) {
                    return false;
                }
                pred = curr;
                goLeft = key < curr->val;
                Node* next = goLeft ? curr->left.load(std::memory_order_acquire)
                                    : curr->right.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    helpUnlink(curr, goLeft);
                    break;
                }
                curr = get_unmarked_ref(next);
            }
            if (!curr) {
                Node* newNode = new Node(key);
                newNode->left.store(nullptr, std::memory_order_relaxed);
                newNode->right.store(nullptr, std::memory_order_relaxed);
                std::atomic<Node*>& link = pred ?
                    (goLeft ? pred->left : pred->right) : root;
                Node* expected = nullptr;
                if (link.compare_exchange_strong(expected, newNode,
                                                 std::memory_order_release, std::memory_order_relaxed)) {
                    return true;
                }
                delete newNode;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = get_unmarked_ref(root.load(std::memory_order_acquire));
            bool goLeft = false;
            while (curr) {
                if (key == curr->val) {
                    if (is_marked_ref(
                            goLeft ? pred->left.load(std::memory_order_acquire)
                                   : pred->right.load(std::memory_order_acquire))) {
                        helpUnlink(pred, goLeft);
                        return false;
                    }
                    std::atomic<Node*>& link = pred ?
                        (goLeft ? pred->left : pred->right) : root;
                    Node* expected = curr;
                    if (link.compare_exchange_strong(expected,
                                                     get_marked_ref(curr),
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                        // Node has been marked
                        return true;
                    }
                    break;
                }
                pred = curr;
                goLeft = key < curr->val;
                Node* next = goLeft ? curr->left.load(std::memory_order_acquire)
                                    : curr->right.load(std::memory_order_acquire);
                if (is_marked_ref(next)) {
                    helpUnlink(curr, goLeft);
                    break;
                }
                curr = get_unmarked_ref(next);
            }
            return false;
        }
    }
};