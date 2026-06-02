#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
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

    ConcurrentDataStructure() {
        Node* sentinelMin = new Node(INT_MIN);
        Node* sentinelMax = new Node(INT_MAX);
        sentinelMin->left = sentinelMin;
        sentinelMin->right = sentinelMax;
        sentinelMax->left = sentinelMin;
        sentinelMax->right = sentinelMax;
        head = sentinelMin;
    }

    ~ConcurrentDataStructure() {
        while (true) {
            Node* node = head.load(std::memory_order_acquire);
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            if (node == next) {
                delete node;
                break;
            }
            delete node;
            head.store(next, std::memory_order_release);
        }
    }

    bool contains(int key) {
        while (true) {
            Node* node = head.load(std::memory_order_acquire);
            Node* next = searchHelper(node, key);
            if (next == nullptr || next->val != key) {
                return false;
            }
            if (!is_marked_ref(next)) {
                return true;
            }
        }
    }

    bool add(int key) {
        while (true) {
            Node* node = head.load(std::memory_order_acquire);
            Node* next = searchHelper(node, key);
            if (next != nullptr && next->val == key) {
                if (!is_marked_ref(next)) {
                    return false;
                }
                if (!next->right.compare_exchange_strong(nullptr, get_marked_ref(nullptr), std::memory_order_acq_rel)) {
                    continue;
                }
                return true;
            }
            Node* newNode = new Node(key);
            newNode->left = next;
            newNode->right = next->right.load(std::memory_order_acquire);
            if (next->right.compare_exchange_strong(newNode->right, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* node = head.load(std::memory_order_acquire);
            Node* next = searchHelper(node, key);
            if (next == nullptr || next->val != key) {
                return false;
            }
            if (is_marked_ref(next)) {
                return true;
            }
            Node* right = next->right.load(std::memory_order_acquire);
            if (right == nullptr) {
                if (next->right.compare_exchange_strong(nullptr, get_marked_ref(nullptr), std::memory_order_acq_rel)) {
                    return true;
                }
            } else {
                Node* nextNext = get_unmarked_ref(right->left.load(std::memory_order_acquire));
                if (nextNext != next) {
                    if (right->left.compare_exchange_strong(next, nextNext, std::memory_order_acq_rel)) {
                        return true;
                    }
                } else {
                    if (next->right.compare_exchange_strong(right, get_marked_ref(right), std::memory_order_acq_rel)) {
                        return true;
                    }
                }
            }
        }
    }

private:
    std::atomic<Node*> head;

    Node* searchHelper(Node* node, int key) {
        while (true) {
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            if (next == nullptr || next->val >= key) {
                return node;
            }
            node = next;
        }
    }
};