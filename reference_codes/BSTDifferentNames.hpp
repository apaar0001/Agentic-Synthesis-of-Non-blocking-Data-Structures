#pragma once
#include <atomic>
#include <limits>

class BSTDifferentNames {
private:
    struct TreeNode {
        const int data;
        std::atomic<bool> active; // true = in set, false = deleted
        std::atomic<TreeNode*> leftNode;
        std::atomic<TreeNode*> rightNode;

        TreeNode(int d) : data(d), active(true), leftNode(nullptr), rightNode(nullptr) {}
    };

    std::atomic<TreeNode*> top{nullptr};

public:
    BSTDifferentNames() = default;

    bool add(int key) {
        while (true) {
            TreeNode* n = new TreeNode(key);
            TreeNode* expected = nullptr;
            if (top.compare_exchange_strong(expected, n)) {
                return true;
            }
            delete n;
            
            TreeNode* c = top.load();
            while (true) {
                if (c->data == key) {
                    bool expected_active = false;
                    if (c->active.compare_exchange_strong(expected_active, true)) {
                        return true;
                    }
                    return false;
                }
                std::atomic<TreeNode*>* next = (key < c->data) ? &c->leftNode : &c->rightNode;
                TreeNode* t = next->load();
                if (t == nullptr) {
                    TreeNode* n_inner = new TreeNode(key);
                    TreeNode* exp_inner = nullptr;
                    if (next->compare_exchange_strong(exp_inner, n_inner)) {
                        return true;
                    }
                    delete n_inner;
                    t = next->load();
                }
                c = t;
            }
        }
    }

    bool remove(int key) {
        TreeNode* c = top.load();
        while (c != nullptr) {
            if (c->data == key) {
                bool expected_active = true;
                if (c->active.compare_exchange_strong(expected_active, false)) {
                    return true;
                }
                return false;
            }
            c = (key < c->data) ? c->leftNode.load() : c->rightNode.load();
        }
        return false;
    }

    bool contains(int key) {
        TreeNode* c = top.load();
        while (c != nullptr) {
            if (c->data == key) {
                return c->active.load();
            }
            c = (key < c->data) ? c->leftNode.load() : c->rightNode.load();
        }
        return false;
    }
};
