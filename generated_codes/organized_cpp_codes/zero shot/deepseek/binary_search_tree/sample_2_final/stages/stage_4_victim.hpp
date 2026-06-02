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

    struct SearchResult {
        Node* parent;
        Node* target;
        Node* gp;
    };

    SearchResult search(int key) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* parent = nullptr;
        Node* gp = nullptr;
        Node* curr = root.load(std::memory_order_acquire);

        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == nullptr) break;

            if (is_marked_ref(curr)) {
                if (parent == nullptr) {
                    Node* next = unmarked_curr;
                    root.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    curr = root.load(std::memory_order_acquire);
                    continue;
                } else {
                    Node* next = unmarked_curr;
                    if (parent->left.load(std::memory_order_relaxed) == curr) {
                        parent->left.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    } else {
                        parent->right.compare_exchange_strong(curr, next, std::memory_order_acq_rel);
                    }
                    curr = parent;
                    parent = gp;
                    gp = nullptr;
                    continue;
                }
            }

            if (unmarked_curr->val == key) {
                result.parent = parent;
                result.target = curr;
                result.gp = gp;
                return result;
            }

            gp = parent;
            parent = curr;
            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }

        result.parent = parent;
        result.target = curr;
        result.gp = gp;
        return result;
    }

public:
    ConcurrentDataStructure() {
        root.store(nullptr, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* curr = root.load(std::memory_order_acquire);
        if (curr == nullptr) return;

        Node* unmarked = get_unmarked_ref(curr);
        if (unmarked != nullptr) {
            delete unmarked;
        }
    }

    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            if (unmarked_curr == nullptr) break;

            if (is_marked_ref(curr)) {
                curr = unmarked_curr;
                continue;
            }

            if (unmarked_curr->val == key) {
                return !is_marked_ref(curr);
            }

            if (key < unmarked_curr->val) {
                curr = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked_curr->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);
            Node* parent = res.parent;
            Node* target = res.target;

            if (target != nullptr && !is_marked_ref(target)) {
                Node* unmarked_target = get_unmarked_ref(target);
                if (unmarked_target != nullptr && unmarked_target->val == key) {
                    return false;
                }
            }

            Node* new_node = new Node(key);
            if (parent == nullptr) {
                if (root.compare_exchange_strong(target, new_node, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete new_node;
                continue;
            }

            Node* unmarked_parent = get_unmarked_ref(parent);
            if (unmarked_parent == nullptr) continue;

            std::atomic<Node*>* child_ptr = nullptr;
            if (key < unmarked_parent->val) {
                child_ptr = &unmarked_parent->left;
            } else {
                child_ptr = &unmarked_parent->right;
            }

            Node* expected = target;
            if (child_ptr->compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            delete new_node;
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            Node* parent = res.parent;
            Node* target = res.target;

            if (target == nullptr || is_marked_ref(target)) {
                return false;
            }

            Node* unmarked_target = get_unmarked_ref(target);
            if (unmarked_target == nullptr || unmarked_target->val != key) {
                return false;
            }

            Node* marked_target = get_marked_ref(target);
            if (parent == nullptr) {
                if (root.compare_exchange_strong(target, marked_target, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                continue;
            }

            Node* unmarked_parent = get_unmarked_ref(parent);
            if (unmarked_parent == nullptr) continue;

            std::atomic<Node*>* child_ptr = nullptr;
            if (key < unmarked_parent->val) {
                child_ptr = &unmarked_parent->left;
            } else {
                child_ptr = &unmarked_parent->right;
            }

            Node* expected = target;
            if (child_ptr->compare_exchange_strong(expected, marked_target, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }
};