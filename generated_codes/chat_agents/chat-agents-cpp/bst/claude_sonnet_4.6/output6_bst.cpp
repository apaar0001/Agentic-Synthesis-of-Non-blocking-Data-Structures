#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <vector>
#include <thread>

class ConcurrentDataStructure : public SetADT {
    struct Node {
        int val;
        std::atomic<Node*> left;
        std::atomic<Node*> right;
        std::atomic<bool>  retired;
        Node(int v) : val(v), left(nullptr), right(nullptr), retired(false) {}
    };

    static bool is_marked(Node* p) { return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0; }
    static Node* unmark(Node* p)   { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL); }
    static Node* mark(Node* p)     { return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL); }

    std::atomic<Node*> root;
    std::atomic<uint64_t> global_epoch{0};

    static thread_local std::vector<Node*> retire_list;

    void reclaim() {
        for (Node* n : retire_list) delete n;
        retire_list.clear();
    }

    void retire(Node* n) {
        if (n) { n->retired.store(true, std::memory_order_release); retire_list.push_back(n); }
        if (retire_list.size() > 64) reclaim();
    }

    void destroy(Node* n) {
        if (!n) return;
        destroy(unmark(n->left.load(std::memory_order_relaxed)));
        destroy(unmark(n->right.load(std::memory_order_relaxed)));
        delete n;
    }

public:
    ConcurrentDataStructure() {
        Node* s = new Node(INT_MIN);
        s->right.store(new Node(INT_MAX), std::memory_order_relaxed);
        root.store(s, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        reclaim();
        destroy(unmark(root.load(std::memory_order_relaxed)));
    }

    bool contains(int key) override {
        Node* cur = unmark(root.load(std::memory_order_acquire));
        while (cur) {
            if (cur->retired.load(std::memory_order_acquire)) {
                cur = unmark(root.load(std::memory_order_acquire));
                continue;
            }
            if (key == cur->val) return true;
            cur = unmark((key < cur->val ? cur->left : cur->right)
                         .load(std::memory_order_acquire));
        }
        return false;
    }

    bool add(int key) override {
        Node* new_node = new Node(key);
        while (true) {
            Node* par  = nullptr;
            Node* cur  = unmark(root.load(std::memory_order_acquire));
            std::atomic<Node*>* slot = &root;

            while (cur) {
                if (key == cur->val) { delete new_node; return false; }
                par  = cur;
                slot = (key < cur->val) ? &cur->left : &cur->right;
                cur  = unmark(slot->load(std::memory_order_acquire));
            }
            Node* exp = nullptr;
            if (slot->compare_exchange_strong(exp, new_node, std::memory_order_acq_rel))
                return true;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* par  = nullptr;
            Node* cur  = unmark(root.load(std::memory_order_acquire));
            std::atomic<Node*>* slot = &root;

            while (cur && cur->val != key) {
                par  = cur;
                slot = (key < cur->val) ? &cur->left : &cur->right;
                cur  = unmark(slot->load(std::memory_order_acquire));
            }
            if (!cur) return false;

            Node* left  = unmark(cur->left.load(std::memory_order_acquire));
            Node* right = unmark(cur->right.load(std::memory_order_acquire));

            Node* marked = mark(cur);
            Node* exp = cur;
            if (!slot->compare_exchange_strong(exp, marked, std::memory_order_acq_rel)) continue;

            Node* replacement = right ? right : left;
            Node* exp2 = marked;
            slot->compare_exchange_strong(exp2, replacement, std::memory_order_acq_rel);
            retire(cur);
            global_epoch.fetch_add(1, std::memory_order_acq_rel);
            return true;
        }
    }
};

thread_local std::vector<ConcurrentDataStructure::Node*> ConcurrentDataStructure::retire_list;
