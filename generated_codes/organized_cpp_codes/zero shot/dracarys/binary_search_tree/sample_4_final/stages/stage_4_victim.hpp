#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
public:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    ConcurrentDataStructure() : head(new Node(INT_MIN)) {
        head->right = new Node(INT_MAX);
    }

    ~ConcurrentDataStructure() {
        deleteTree(head);
    }

    bool contains(int key) {
        Node* node = head;
        while (true) {
            node = getUnmarkedRef(node->right);
            if (node->val == key) return true;
            if (node->val > key) return false;
            node = getUnmarkedRef(node->right);
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = head;
            Node* node = getUnmarkedRef(parent->right);
            while (node->val < key) {
                parent = node;
                node = getUnmarkedRef(node->right);
            }
            if (node->val == key) return false;
            Node* newNode = new Node(key);
            newNode->right = node;
            newNode->left = parent;
            if (parent->right.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = head;
            Node* node = getUnmarkedRef(parent->right);
            while (node->val < key) {
                parent = node;
                node = getUnmarkedRef(node->right);
            }
            if (node->val != key) return false;
            Node* next = getUnmarkedRef(node->right);
            if (next->val > key) {
                node->right.compare_exchange_strong(next, getMarkedRef(next), std::memory_order_acq_rel);
                continue;
            }
            Node* newNext = next->right;
            node->right.compare_exchange_strong(next, newNext, std::memory_order_acq_rel);
            parent->right.compare_exchange_strong(node, newNext, std::memory_order_acq_rel);
            return true;
        }
    }

private:
    Node* head;

    static bool isMarkedRef(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }

    static Node* getUnmarkedRef(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }

    static Node* getMarkedRef(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    void deleteTree(Node* node) {
        if (node == nullptr) return;
        deleteTree(getUnmarkedRef(node->left));
        deleteTree(getUnmarkedRef(node->right));
        delete node;
    }
};