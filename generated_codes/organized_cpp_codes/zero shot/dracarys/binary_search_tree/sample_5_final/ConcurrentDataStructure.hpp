#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
public:
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
        Node* current = get_unmarked_ref(head->right.load(std::memory_order_relaxed));
        while (current != head) {
            Node* next = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
            delete current;
            current = next;
        }
        delete head;
    }

    bool contains(int key) {
        Node* parent = head;
        Node* current = get_unmarked_ref(parent->right.load(std::memory_order_relaxed));
        while (current != head) {
            if (is_marked_ref(current)) {
                current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
                continue;
            }
            if (current->val == key) return true;
            if (current->val < key) {
                parent = current;
                current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
            } else {
                parent = current;
                current = get_unmarked_ref(current->left.load(std::memory_order_relaxed));
            }
        }
        return false;
    }

    bool add(int key) {
        while (true) {
            Node* parent = head;
            Node* current = get_unmarked_ref(parent->right.load(std::memory_order_relaxed));
            while (current != head) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
                    continue;
                }
                if (current->val == key) return false;
                if (current->val < key) {
                    parent = current;
                    current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
                } else {
                    parent = current;
                    current = get_unmarked_ref(current->left.load(std::memory_order_relaxed));
                }
            }
            Node* newNode = new Node(key);
            newNode->left = current;
            newNode->right = current;
            if (parent->val < key) {
                if (parent->right.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) return true;
            } else {
                if (parent->left.compare_exchange_strong(current, newNode, std::memory_order_acq_rel)) return true;
            }
        }
    }

    bool remove(int key) {
        while (true) {
            Node* parent = head;
            Node* current = get_unmarked_ref(parent->right.load(std::memory_order_relaxed));
            while (current != head) {
                if (is_marked_ref(current)) {
                    current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
                    continue;
                }
                if (current->val == key) break;
                if (current->val < key) {
                    parent = current;
                    current = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
                } else {
                    parent = current;
                    current = get_unmarked_ref(current->left.load(std::memory_order_relaxed));
                }
            }
            if (current == head || current->val != key) return false;
            Node* leftChild = get_unmarked_ref(current->left.load(std::memory_order_relaxed));
            Node* rightChild = get_unmarked_ref(current->right.load(std::memory_order_relaxed));
            if (leftChild == current || rightChild == current) {
                if (parent->val < key) {
                    if (parent->right.compare_exchange_strong(current, get_marked_ref(current), std::memory_order_acq_rel)) {
                        delete current;
                        return true;
                    }
                } else {
                    if (parent->left.compare_exchange_strong(current, get_marked_ref(current), std::memory_order_acq_rel)) {
                        delete current;
                        return true;
                    }
                }
            } else {
                Node* successor = rightChild;
                Node* successorParent = current;
                while (get_unmarked_ref(successor->left.load(std::memory_order_relaxed)) != successor) {
                    successorParent = successor;
                    successor = get_unmarked_ref(successor->left.load(std::memory_order_relaxed));
                }
                if (successorParent->val < key) {
                    if (successorParent->left.compare_exchange_strong(successor, get_marked_ref(successor), std::memory_order_acq_rel)) {
                        delete successor;
                        if (parent->val < key) {
                            if (parent->right.compare_exchange_strong(current, rightChild, std::memory_order_acq_rel)) {
                                delete current;
                                return true;
                            }
                        } else {
                            if (parent->left.compare_exchange_strong(current, rightChild, std::memory_order_acq_rel)) {
                                delete current;
                                return true;
                            }
                        }
                    }
                } else {
                    if (successorParent->left.compare_exchange_strong(successor, get_marked_ref(successor), std::memory_order_acq_rel)) {
                        delete successor;
                        if (parent->val < key) {
                            if (parent->right.compare_exchange_strong(current, leftChild, std::memory_order_acq_rel)) {
                                delete current;
                                return true;
                            }
                        } else {
                            if (parent->left.compare_exchange_strong(current, leftChild, std::memory_order_acq_rel)) {
                                delete current;
                                return true;
                            }
                        }
                    }
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

    std::atomic<Node*> head;
};