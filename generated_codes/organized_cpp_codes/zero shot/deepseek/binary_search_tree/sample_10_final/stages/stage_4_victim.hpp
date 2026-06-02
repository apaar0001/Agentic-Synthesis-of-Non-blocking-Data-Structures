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

    struct SearchResult {
        Node* parent;
        Node* target;
        Node* gp;
    };

    std::atomic<Node*> root;

    SearchResult search(int key, Node* start = nullptr) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* curr = start ? start : root.load(std::memory_order_acquire);
        Node* prev = nullptr;
        Node* gprev = nullptr;

        while (curr != nullptr) {
            Node* unmarked_curr = get_unmarked_ref(curr);
            Node* next = nullptr;

            if (unmarked_curr->val == key) {
                result.target = unmarked_curr;
                result.parent = prev;
                result.gp = gprev;
                return result;
            }

            if (key < unmarked_curr->val) {
                next = unmarked_curr->left.load(std::memory_order_acquire);
            } else {
                next = unmarked_curr->right.load(std::memory_order_acquire);
            }

            if (is_marked_ref(next)) {
                Node* unmarked_next = get_unmarked_ref(next);
                if (prev == nullptr) {
                    root.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel);
                    curr = root.load(std::memory_order_acquire);
                    prev = nullptr;
                    gprev = nullptr;
                    continue;
                } else {
                    if (key < prev->val) {
                        prev->left.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel);
                    } else {
                        prev->right.compare_exchange_strong(curr, unmarked_next, std::memory_order_acq_rel);
                    }
                    curr = unmarked_next;
                    continue;
                }
            }

            gprev = prev;
            prev = unmarked_curr;
            curr = next;
        }

        result.parent = prev;
        result.target = nullptr;
        result.gp = gprev;
        return result;
    }

    void cleanup(Node* node) {
        if (!node) return;
        Node* left = node->left.load(std::memory_order_acquire);
        Node* right = node->right.load(std::memory_order_acquire);
        if (left && !is_marked_ref(left)) {
            cleanup(get_unmarked_ref(left));
        }
        if (right && !is_marked_ref(right)) {
            cleanup(get_unmarked_ref(right));
        }
        delete node;
    }

public:
    ConcurrentDataStructure() {
        root.store(new Node(INT_MAX), std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_acquire);
        cleanup(get_unmarked_ref(r));
    }

    bool contains(int key) override {
        SearchResult res = search(key);
        return res.target != nullptr;
    }

    bool add(int key) override {
        while (true) {
            SearchResult res = search(key);
            if (res.target != nullptr) {
                return false;
            }

            Node* new_node = new Node(key);
            Node* parent = res.parent;

            if (key < parent->val) {
                Node* expected = parent->left.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) {
                    delete new_node;
                    continue;
                }
                new_node->left.store(expected, std::memory_order_relaxed);
                if (parent->left.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete new_node;
            } else {
                Node* expected = parent->right.load(std::memory_order_acquire);
                if (is_marked_ref(expected)) {
                    delete new_node;
                    continue;
                }
                new_node->right.store(expected, std::memory_order_relaxed);
                if (parent->right.compare_exchange_strong(expected, new_node, std::memory_order_acq_rel)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete new_node;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            SearchResult res = search(key);
            Node* target = res.target;
            if (target == nullptr) {
                return false;
            }

            Node* left = target->left.load(std::memory_order_acquire);
            Node* right = target->right.load(std::memory_order_acquire);

            Node* marked_left = get_marked_ref(left);
            if (target->left.compare_exchange_strong(left, marked_left, std::memory_order_acq_rel)) {
                std::this_thread::sleep_for(std::chrono::seconds(5));
                Node* marked_right = get_marked_ref(right);
                target->right.compare_exchange_strong(right, marked_right, std::memory_order_acq_rel);

                Node* parent = res.parent;
                if (parent == nullptr) {
                    Node* unmarked_left = get_unmarked_ref(marked_left);
                    root.compare_exchange_strong(target, unmarked_left, std::memory_order_acq_rel);
                } else {
                    if (key < parent->val) {
                        Node* unmarked_left = get_unmarked_ref(marked_left);
                        parent->left.compare_exchange_strong(target, unmarked_left, std::memory_order_acq_rel);
                    } else {
                        Node* unmarked_left = get_unmarked_ref(marked_left);
                        parent->right.compare_exchange_strong(target, unmarked_left, std::memory_order_acq_rel);
                    }
                }
                return true;
            }
        }
    }
};