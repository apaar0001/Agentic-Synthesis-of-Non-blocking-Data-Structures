#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : head_(new Node(INT_MIN)), tail_(new Node(INT_MAX)) {
        head_->left = head_;
        head_->right = tail_;
        tail_->left = head_;
        tail_->right = tail_;
    }

    ~ConcurrentDataStructure() {
        while (true) {
            Node* node = head_.load(std::memory_order_relaxed);
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            if (next == tail_) {
                break;
            }
            delete node;
            head_.store(next, std::memory_order_relaxed);
        }
        delete tail_;
    }

    bool contains(int key) {
        while (true) {
            Node* node = head_.load(std::memory_order_relaxed);
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            while (next != tail_) {
                if (next->val == key) {
                    return true;
                }
                node = next;
                next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            }
            if (node->val == key) {
                return true;
            }
            return false;
        }
    }

    bool add(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = search(key, parent);
            if (node != nullptr) {
                return false;
            }
            Node* newNode = new Node(key);
            newNode->left = head_;
            newNode->right = parent;
            if (parent->right.compare_exchange_strong(node, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = nullptr;
            Node* node = search(key, parent);
            if (node == nullptr) {
                return false;
            }
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            if (next == tail_) {
                return false;
            }
            Node* markedNext = get_marked_ref(next);
            if (node->right.compare_exchange_strong(next, markedNext, std::memory_order_acq_rel)) {
                Node* newNext = get_unmarked_ref(next->right.load(std::memory_order_relaxed));
                if (parent->right.compare_exchange_strong(node, newNext, std::memory_order_acq_rel)) {
                    delete node;
                    return true;
                }
            }
        }
    }

private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        Node(int val) : val(val) {}
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

    Node* search(int key, Node*& parent) {
        while (true) {
            Node* node = head_.load(std::memory_order_relaxed);
            Node* next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            while (next != tail_) {
                if (next->val == key) {
                    return next;
                }
                parent = node;
                node = next;
                next = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
            }
            if (node->val == key) {
                return node;
            }
            return nullptr;
        }
    }

    std::atomic<Node*> head_;
    Node* tail_;
};