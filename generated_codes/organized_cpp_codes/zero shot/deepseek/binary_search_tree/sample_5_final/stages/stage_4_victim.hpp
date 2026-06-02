#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;

        Node(int v, Node* l = nullptr, Node* r = nullptr) : val(v), left(l), right(r) {}
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

    std::atomic<Node*> root;

    bool search(int key, Node** parent, Node** target, Node** grandparent = nullptr) {
        Node* curr = root.load(std::memory_order_acquire);
        Node* prev = nullptr;
        Node* gprev = nullptr;

        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (is_marked_ref(curr)) {
                if (prev != nullptr) {
                    Node* prev_left = prev->left.load(std::memory_order_relaxed);
                    Node* prev_right = prev->right.load(std::memory_order_relaxed);
                    Node* unmarked_prev_left = get_unmarked_ref(prev_left);
                    Node* unmarked_prev_right = get_unmarked_ref(prev_right);

                    if (unmarked_prev_left == unmarked_curr) {
                        Node* new_child = unmarked_curr->right.load(std::memory_order_acquire);
                        prev->left.compare_exchange_strong(prev_left, new_child,
                            std::memory_order_acq_rel, std::memory_order_relaxed);
                    } else if (unmarked_prev_right == unmarked_curr) {
                        Node* new_child = unmarked_curr->left.load(std::memory_order_acquire);
                        prev->right.compare_exchange_strong(prev_right, new_child,
                            std::memory_order_acq_rel, std::memory_order_relaxed);
                    }
                }
                curr = root.load(std::memory_order_acquire);
                prev = nullptr;
                gprev = nullptr;
                continue;
            }

            if (unmarked_curr->val == key) {
                if (parent) *parent = prev;
                if (target) *target = unmarked_curr;
                if (grandparent) *grandparent = gprev;
                return true;
            }

            gprev = prev;
            prev = unmarked_curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }

        if (parent) *parent = prev;
        if (target) *target = nullptr;
        if (grandparent) *grandparent = gprev;
        return false;
    }

    void cleanup(Node* start) {
        if (!start) return;
        Node* left = start->left.load(std::memory_order_relaxed);
        Node* right = start->right.load(std::memory_order_relaxed);
        cleanup(get_unmarked_ref(left));
        cleanup(get_unmarked_ref(right));
        delete start;
    }

public:
    ConcurrentDataStructure() {
        Node* r = new Node(INT_MAX);
        r->left.store(new Node(INT_MIN), std::memory_order_relaxed);
        root.store(r, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_relaxed);
        cleanup(r);
    }

    bool contains(int key) override {
        Node* parent = nullptr;
        Node* target = nullptr;
        return search(key, &parent, &target);
    }

    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* target = nullptr;
            if (search(key, &parent, &target)) {
                return false;
            }

            Node* new_node = new Node(key);
            Node* expected = nullptr;

            if (key < parent->val) {
                expected = parent->left.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) {
                    delete new_node;
                    continue;
                }
                if (parent->left.compare_exchange_strong(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
            } else {
                expected = parent->right.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) {
                    delete new_node;
                    continue;
                }
                if (parent->right.compare_exchange_strong(expected, new_node,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* target = nullptr;
            Node* grandparent = nullptr;
            if (!search(key, &parent, &target, &grandparent)) {
                return false;
            }

            Node* target_left = target->left.load(std::memory_order_acquire);
            Node* target_right = target->right.load(std::memory_order_acquire);

            if (is_marked_ref(target_left) || is_marked_ref(target_right)) {
                continue;
            }

            Node* marked_left = get_marked_ref(target_left);
            if (!target->left.compare_exchange_strong(target_left, marked_left,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                continue;
            }

            Node* marked_right = get_marked_ref(target_right);
            if (!target->right.compare_exchange_strong(target_right, marked_right,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
                target->left.compare_exchange_strong(marked_left, target_left,
                    std::memory_order_relaxed, std::memory_order_relaxed);
                continue;
            }

            Node* new_child = nullptr;
            if (target_left != nullptr && target_right != nullptr) {
                Node* successor = get_unmarked_ref(target_right);
                Node* succ_parent = target;
                while (true) {
                    Node* succ_left = successor->left.load(std::memory_order_acquire);
                    if (succ_left == nullptr) break;
                    succ_parent = successor;
                    successor = get_unmarked_ref(succ_left);
                }

                Node* succ_right = successor->right.load(std::memory_order_acquire);
                if (is_marked_ref(succ_right)) {
                    continue;
                }

                Node* marked_succ = get_marked_ref(successor);
                if (succ_parent->left.load(std::memory_order_acquire) == successor) {
                    succ_parent->left.compare_exchange_strong(successor, marked_succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed);
                } else {
                    succ_parent->right.compare_exchange_strong(successor, marked_succ,
                        std::memory_order_acq_rel, std::memory_order_relaxed);
                }

                new_child = get_unmarked_ref(target_right);
            } else if (target_left != nullptr) {
                new_child = get_unmarked_ref(target_left);
            } else if (target_right != nullptr) {
                new_child = get_unmarked_ref(target_right);
            }

            if (parent->left.load(std::memory_order_acquire) == target) {
                parent->left.compare_exchange_strong(target, new_child,
                    std::memory_order_acq_rel, std::memory_order_relaxed);
            } else {
                parent->right.compare_exchange_strong(target, new_child,
                    std::memory_order_acq_rel, std::memory_order_relaxed);
            }

            return true;
        }
    }
};