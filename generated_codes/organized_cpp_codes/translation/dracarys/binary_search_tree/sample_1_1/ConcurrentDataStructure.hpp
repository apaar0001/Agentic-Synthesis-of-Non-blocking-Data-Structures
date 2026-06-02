#pragma once
#include <atomic>
#include <climits>
#include <cstdint>
#include "../utils/SetADT.hpp"

class ConcurrentDataStructure : public SetADT {
public:
    ConcurrentDataStructure() : root(new Node(INT_MIN)) {}

    bool contains(int key) override;
    bool add(int key) override;
    bool remove(int key) override;

private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int val) : val(val), left(nullptr), right(nullptr) {}
    };

    static bool is_marked_ref(uintptr_t ref) {
        return ref & 1;
    }

    static uintptr_t get_unmarked_ref(uintptr_t ref) {
        return ref & ~1;
    }

    static uintptr_t get_marked_ref(uintptr_t ref) {
        return ref | 1;
    }

    Node* root;

    Node* find(Node* node, int key);
    Node* findMin(Node* node);
    bool removeNode(Node* node, int key);
};

ConcurrentDataStructure::Node* ConcurrentDataStructure::find(Node* node, int key) {
    while (true) {
        Node* next = node->left.load(std::memory_order_acquire);
        uintptr_t ref = reinterpret_cast<uintptr_t>(next);
        if (is_marked_ref(ref)) {
            next = reinterpret_cast<Node*>(get_unmarked_ref(ref));
            if (next == nullptr || next->val < key) {
                next = node->right.load(std::memory_order_acquire);
                ref = reinterpret_cast<uintptr_t>(next);
                if (is_marked_ref(ref)) {
                    next = reinterpret_cast<Node*>(get_unmarked_ref(ref));
                }
            }
        }
        if (next == nullptr || next->val >= key) {
            return node;
        }
        node = next;
    }
}

ConcurrentDataStructure::Node* ConcurrentDataStructure::findMin(Node* node) {
    while (true) {
        Node* next = node->left.load(std::memory_order_acquire);
        uintptr_t ref = reinterpret_cast<uintptr_t>(next);
        if (is_marked_ref(ref)) {
            next = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (next == nullptr) {
            return node;
        }
        node = next;
    }
}

bool ConcurrentDataStructure::removeNode(Node* node, int key) {
    Node* parent = find(node, key);
    Node* current = parent->left.load(std::memory_order_acquire);
    uintptr_t ref = reinterpret_cast<uintptr_t>(current);
    if (is_marked_ref(ref)) {
        current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
    }
    if (current == nullptr || current->val != key) {
        current = parent->right.load(std::memory_order_acquire);
        ref = reinterpret_cast<uintptr_t>(current);
        if (is_marked_ref(ref)) {
            current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (current == nullptr || current->val != key) {
            return false;
        }
    }
    Node* next = nullptr;
    if (current->left.load(std::memory_order_acquire) == nullptr) {
        next = current->right.load(std::memory_order_acquire);
    } else if (current->right.load(std::memory_order_acquire) == nullptr) {
        next = current->left.load(std::memory_order_acquire);
    } else {
        Node* minNode = findMin(current->right.load(std::memory_order_acquire));
        next = minNode->right.load(std::memory_order_acquire);
        ref = reinterpret_cast<uintptr_t>(next);
        if (is_marked_ref(ref)) {
            next = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        current->val = minNode->val;
        current = minNode;
    }
    ref = reinterpret_cast<uintptr_t>(current);
    if (is_marked_ref(ref)) {
        return false;
    }
    uintptr_t markedRef = get_marked_ref(ref);
    Node* markedCurrent = reinterpret_cast<Node*>(markedRef);
    if (parent->left.compare_exchange_strong(current, markedCurrent, std::memory_order_acq_rel)) {
        // Node has been marked
        if (next != nullptr) {
            parent->left.compare_exchange_strong(markedCurrent, next, std::memory_order_acq_rel);
        } else {
            parent->left.compare_exchange_strong(markedCurrent, nullptr, std::memory_order_acq_rel);
        }
    }
    return true;
}

bool ConcurrentDataStructure::contains(int key) {
    while (true) {
        Node* node = find(root, key);
        Node* current = node->left.load(std::memory_order_acquire);
        uintptr_t ref = reinterpret_cast<uintptr_t>(current);
        if (is_marked_ref(ref)) {
            current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (current != nullptr && current->val == key) {
            return true;
        }
        current = node->right.load(std::memory_order_acquire);
        ref = reinterpret_cast<uintptr_t>(current);
        if (is_marked_ref(ref)) {
            current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (current != nullptr && current->val == key) {
            return true;
        }
        if (node->val == key) {
            return true;
        }
    }
}

bool ConcurrentDataStructure::add(int key) {
    while (true) {
        Node* node = find(root, key);
        Node* current = node->left.load(std::memory_order_acquire);
        uintptr_t ref = reinterpret_cast<uintptr_t>(current);
        if (is_marked_ref(ref)) {
            current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (current != nullptr && current->val == key) {
            return false;
        }
        current = node->right.load(std::memory_order_acquire);
        ref = reinterpret_cast<uintptr_t>(current);
        if (is_marked_ref(ref)) {
            current = reinterpret_cast<Node*>(get_unmarked_ref(ref));
        }
        if (current != nullptr && current->val == key) {
            return false;
        }
        Node* newNode = new Node(key);
        if (key < node->val) {
            Node* expected = nullptr;
            if (node->left.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        } else {
            Node* expected = nullptr;
            if (node->right.compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                return true;
            }
        }
    }
}

bool ConcurrentDataStructure::remove(int key) {
    while (true) {
        if (removeNode(root, key)) {
            return true;
        }
    }
}