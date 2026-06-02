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
        
        explicit Node(int key) : val(key), left(nullptr), right(nullptr) {}
    };
    
    std::atomic<Node*> root;
    
    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }
    
    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }
    
    bool helpMark(Node* node, bool markLeft) {
        std::atomic<Node*>* target = markLeft ? &node->left : &node->right;
        Node* expected = target->load(std::memory_order_acquire);
        while (true) {
            if (is_marked_ref(expected)) return false;
            Node* desired = get_marked_ref(expected);
            if (target->compare_exchange_strong(expected, desired,
                                                std::memory_order_acq_rel,
                                                std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
    
    bool helpUnlink(Node* parent, Node* child, bool isLeft) {
        std::atomic<Node*>* target = isLeft ? &parent->left : &parent->right;
        Node* expected = child;
        Node* unmarkedChild = get_unmarked_ref(child);
        if (!unmarkedChild) return false;
        Node* newChild = unmarkedChild->right.load(std::memory_order_acquire);
        return target->compare_exchange_strong(expected, newChild,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
    }
    
    void cleanup(Node* node) {
        if (!node) return;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_relaxed));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_relaxed));
        if (left) cleanup(left);
        if (right) cleanup(right);
        delete node;
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_relaxed);
        cleanup(get_unmarked_ref(r));
    }
    
    bool contains(int key) override {
        Node* curr = root.load(std::memory_order_acquire);
        while (curr) {
            Node* unmarked = get_unmarked_ref(curr);
            if (!unmarked) break;
            
            if (key == unmarked->val) {
                return !is_marked_ref(curr);
            } else if (key < unmarked->val) {
                curr = unmarked->left.load(std::memory_order_acquire);
            } else {
                curr = unmarked->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* parentPtr = &root;
            Node* curr = root.load(std::memory_order_acquire);
            
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (!unmarked) {
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parentPtr = &root;
                    continue;
                }
                
                if (key == unmarked->val) {
                    if (!is_marked_ref(curr)) {
                        return false;
                    }
                    if (parent) {
                        helpUnlink(parent, curr, parentPtr == &parent->left);
                    }
                    break;
                }
                
                parent = unmarked;
                if (key < unmarked->val) {
                    parentPtr = &parent->left;
                } else {
                    parentPtr = &parent->right;
                }
                curr = parentPtr->load(std::memory_order_acquire);
            }
            
            if (!curr) {
                Node* newNode = new Node(key);
                Node* expected = nullptr;
                if (parentPtr->compare_exchange_strong(expected, newNode,
                                                       std::memory_order_acq_rel,
                                                       std::memory_order_acquire)) {
                    return true;
                }
                delete newNode;
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            std::atomic<Node*>* parentPtr = &root;
            Node* curr = root.load(std::memory_order_acquire);
            Node* target = nullptr;
            Node* targetUnmarked = nullptr;
            
            while (curr) {
                Node* unmarked = get_unmarked_ref(curr);
                if (!unmarked) {
                    curr = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parentPtr = &root;
                    continue;
                }
                
                if (key == unmarked->val) {
                    target = curr;
                    targetUnmarked = unmarked;
                    break;
                }
                
                parent = unmarked;
                if (key < unmarked->val) {
                    parentPtr = &parent->left;
                } else {
                    parentPtr = &parent->right;
                }
                curr = parentPtr->load(std::memory_order_acquire);
            }
            
            if (!target) return false;
            
            if (is_marked_ref(target)) {
                if (parent) {
                    helpUnlink(parent, target, parentPtr == &parent->left);
                }
                continue;
            }
            
            bool markedLeft = helpMark(targetUnmarked, true);
            bool markedRight = helpMark(targetUnmarked, false);
            
            if (markedLeft || markedRight) {
                // Node has been marked
                if (parent) {
                    helpUnlink(parent, target, parentPtr == &parent->left);
                } else {
                    Node* expected = target;
                    Node* newChild = targetUnmarked->right.load(std::memory_order_acquire);
                    root.compare_exchange_strong(expected, newChild,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
                }
                return true;
            }
        }
    }
};