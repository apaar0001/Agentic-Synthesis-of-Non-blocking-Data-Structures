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

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    Node dummyHead; // dummyHead.left points to the actual root; val = INT_MIN sentinel

    bool find(Node*& parent, std::atomic<Node*>*& parentField, int key,
              Node*& node, std::atomic<Node*>*& nodeField) {
        parent = &dummyHead;
        parentField = &dummyHead.left;
        while (true) {
            Node* markedPtr = parentField->load(std::memory_order_acquire);
            if (is_marked_ref(markedPtr)) {
                // Help unlink the marked node
                Node* expected = markedPtr;
                Node* child = get_unmarked_ref(markedPtr);
                Node* left = get_unmarked_ref(child->left.load(std::memory_order_acquire));
                Node* right = get_unmarked_ref(child->right.load(std::memory_order_acquire));
                Node* replacement = (left != nullptr) ? left : right;
                if (!parentField->compare_exchange_strong(expected, replacement,
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                    return false; // restart
                }
                continue;
            }
            node = get_unmarked_ref(markedPtr);
            nodeField = parentField;
            if (node == nullptr) {
                return true; // insert position
            }
            if (node->val == key)
                return true; // found
            parent = node;
            if (key < node->val) {
                parentField = &node->left;
            } else {
                parentField = &node->right;
            }
        }
    }

    // Find successor (min node in subtree) and its parent
    bool findSuccessor(Node* node, Node*& succParent, std::atomic<Node*>*& succParentField,
                       Node*& succ, std::atomic<Node*>*& succField) {
        succParent = node;
        succParentField = &node->right;
        while (true) {
            Node* markedPtr = succParentField->load(std::memory_order_acquire);
            if (is_marked_ref(markedPtr)) {
                // Help unlink
                Node* expected = markedPtr;
                Node* child = get_unmarked_ref(markedPtr);
                Node* left = get_unmarked_ref(child->left.load(std::memory_order_acquire));
                Node* right = get_unmarked_ref(child->right.load(std::memory_order_acquire));
                Node* replacement = (left != nullptr) ? left : right;
                if (!succParentField->compare_exchange_strong(expected, replacement,
                        std::memory_order_acq_rel, std::memory_order_acquire))
                    return false; // restart
                continue;
            }
            succ = get_unmarked_ref(markedPtr);
            succField = succParentField;
            if (succ == nullptr) {
                return true; // should not happen if node->right exists
            }
            if (succ->left.load(std::memory_order_acquire) == nullptr) {
                return true; // successor found (no left child)
            }
            succParent = succ;
            succParentField = &succ->left;
        }
    }

public:
    ConcurrentDataStructure() : dummyHead(INT_MIN) {
        dummyHead.left.store(nullptr, std::memory_order_relaxed);
        dummyHead.right.store(nullptr, std::memory_order_relaxed);
    }
    ~ConcurrentDataStructure() = default;

    bool contains(int key) override {
        Node* cur = &dummyHead;
        std::atomic<Node*>* field = &dummyHead.left;
        while (true) {
            Node* markedPtr = field->load(std::memory_order_acquire);
            if (is_marked_ref(markedPtr))
                return false; // skipped as logically deleted
            Node* child = get_unmarked_ref(markedPtr);
            if (child == nullptr)
                return false;
            if (child->val == key)
                return true;
            cur = child;
            if (key < child->val)
                field = &child->left;
            else
                field = &child->right;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* parent;
            std::atomic<Node*>* parentField;
            Node* node;
            std::atomic<Node*>* nodeField;
            bool res = find(parent, parentField, key, node, nodeField);
            if (!res)
                continue;
            if (node != nullptr && node->val == key)
                return false; // duplicate
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (parentField->compare_exchange_strong(expected, newNode,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                return true;
            // CAS failed, retry
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent;
            std::atomic<Node*>* parentField;
            Node* node;
            std::atomic<Node*>* nodeField;
            bool res = find(parent, parentField, key, node, nodeField);
            if (!res)
                continue;
            if (node == nullptr || node->val != key)
                return false; // not present
            Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
            Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
            if (left != nullptr && right != nullptr) {
                // Two children: find successor
                Node* succParent;
                std::atomic<Node*>* succParentField;
                Node* succ;
                std::atomic<Node*>* succField;
                bool succRes = findSuccessor(node, succParent, succParentField, succ, succField);
                if (!succRes)
                    continue; // restart due to helping
                if (succ == nullptr)
                    continue; // should not happen
                // Copy successor's value to node
                node->val = succ->val;
                // Now delete successor (which has at most one child)
                Node* succLeft = get_unmarked_ref(succ->left.load(std::memory_order_acquire));
                Node* succRight = get_unmarked_ref(succ->right.load(std::memory_order_acquire));
                Node* succChild = (succLeft != nullptr) ? succLeft : succRight;
                // Attempt to mark successor pointer from its parent
                Node* expected = succField->load(std::memory_order_acquire);
                if (is_marked_ref(expected) || get_unmarked_ref(expected) != succ)
                    continue; // changed, retry
                Node* marked = get_marked_ref(succ);
                bool markedSuccess = succField->compare_exchange_strong(expected, marked,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                if (!markedSuccess)
                    continue; // retry
                // Node has been marked
                // Physical unlink of successor
                Node* expectedMarked = marked;
                bool unlinked = succField->compare_exchange_strong(expectedMarked, succChild,
                        std::memory_order_acq_rel, std::memory_order_acquire);
                // If unlink fails, we leave it marked; a concurrent thread may help.
                return true;
            }
            // Zero or one child case
            Node* replacement = (left != nullptr) ? left : right;
            Node* expected = nodeField->load(std::memory_order_acquire);
            if (is_marked_ref(expected) || get_unmarked_ref(expected) != node)
                continue; // changed, retry
            Node* marked = get_marked_ref(node);
            bool markedSuccess = nodeField->compare_exchange_strong(expected, marked,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            if (!markedSuccess)
                continue; // retry
            // Node has been marked
            // Physical unlink
            Node* expectedMarked = marked;
            bool unlinked = nodeField->compare_exchange_strong(expectedMarked, replacement,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            // If unlink fails, we leave it marked; a concurrent thread may help.
            return true;
        }
    }
};