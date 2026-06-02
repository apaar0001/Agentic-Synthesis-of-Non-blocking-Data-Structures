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
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    struct SearchResult {
        Node* parent;
        std::atomic<Node*>* child_ptr;
        Node* node;
    };

    Node* head;

    void destroy(Node* n) {
        if (!n) return;
        Node* left = get_unmarked_ref(n->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(n->right.load(std::memory_order_acquire));
        destroy(left);
        destroy(right);
        delete n;
    }

    SearchResult find(int key) const {
        Node* parent = head;
        std::atomic<Node*>* child_ptr = (key < head->val) ? &(head->left) : &(head->right);
        while (true) {
            Node* curr = get_unmarked_ref(child_ptr->load(std::memory_order_acquire));
            if (curr == nullptr) {
                return {parent, child_ptr, nullptr};
            }
            if (is_marked_ref(child_ptr->load(std::memory_order_acquire))) {
                helpRemove(parent, child_ptr, curr);
                parent = head;
                child_ptr = (key < head->val) ? &(head->left) : &(head->right);
                continue;
            }
            if (key < curr->val) {
                parent = curr;
                child_ptr = &(curr->left);
            } else if (key > curr->val) {
                parent = curr;
                child_ptr = &(curr->right);
            } else {
                return {parent, child_ptr, curr};
            }
        }
    }

    bool helpRemove(Node* parent, std::atomic<Node*>* child_ptr, Node* victim) {
        Node* left = get_unmarked_ref(victim->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(victim->right.load(std::memory_order_acquire));
        Node* repl = nullptr;
        if (left == nullptr) repl = right;
        else if (right == nullptr) repl = left;
        if (repl == nullptr) return false;

        Node* expected = get_marked_ref(victim);
        return child_ptr->compare_exchange_strong(expected, repl,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_relaxed);
    }

public:
    ConcurrentDataStructure() {
        head = new Node(0); // dummy node value irrelevant
        Node* leftSentinel = new Node(INT_MIN);
        Node* rightSentinel = new Node(INT_MAX);
        head->left.store(leftSentinel, std::memory_order_release);
        head->right.store(rightSentinel, std::memory_order_release);
        leftSentinel->left.store(nullptr, std::memory_order_relaxed);
        leftSentinel->right.store(nullptr, std::memory_order_relaxed);
        rightSentinel->left.store(nullptr, std::memory_order_relaxed);
        rightSentinel->right.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        destroy(get_unmarked_ref(head->left.load(std::memory_order_acquire)));
        destroy(get_unmarked_ref(head->right.load(std::memory_order_acquire)));
        delete head;
    }

    bool contains(int key) const override {
        Node* parent = head;
        std::atomic<Node*>* child_ptr = (key < head->val) ? &(head->left) : &(head->right);
        while (true) {
            Node* curr = get_unmarked_ref(child_ptr->load(std::memory_order_acquire));
            if (curr == nullptr) {
                return false;
            }
            if (is_marked_ref(child_ptr->load(std::memory_order_acquire))) {
                helpRemove(parent, child_ptr, curr);
                parent = head;
                child_ptr = (key < head->val) ? &(head->left) : &(head->right);
                continue;
            }
            if (key < curr->val) {
                parent = curr;
                child_ptr = &(curr->left);
            } else if (key > curr->val) {
                parent = curr;
                child_ptr = &(curr->right);
            } else {
                return true;
            }
        }
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = find(key);
            if (res.node != nullptr) {
                if (!is_marked_ref(*res.child_ptr)) {
                    return false;
                }
                if (helpRemove(res.parent, res.child_ptr, res.node)) {
                    continue;
                } else {
                    continue;
                }
            }
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (res.child_ptr->compare_exchange_strong(expected, newNode,
                                                       std::memory_order_release,
                                                       std::memory_order_relaxed)) {
                return true;
            } else {
                delete newNode;
                continue;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = find(key);
            if (res.node == nullptr) {
                return false;
            }
            if (is_marked_ref(*res.child_ptr)) {
                return false;
            }
            Node* expected = res.node;
            Node* desired = get_marked_ref(res.node);
            if (!res.child_ptr->compare_exchange_strong(expected, desired,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_relaxed)) {
                continue;
            }
            helpRemove(res.parent, res.child_ptr, res.node);
            return true;
        }
    }
};