#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstdint>
#include <climits>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        explicit Node(int v) : val(v), left(nullptr), right(nullptr) {}
    };

    std::atomic<Node*> root{nullptr};

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    void free_nodes(Node* n) {
        if (!n) return;
        Node* left_ptr = n->left.load(std::memory_order_relaxed);
        Node* right_ptr = n->right.load(std::memory_order_relaxed);
        free_nodes(get_unmarked_ref(left_ptr));
        free_nodes(get_unmarked_ref(right_ptr));
        delete n;
    }

public:
    ConcurrentData