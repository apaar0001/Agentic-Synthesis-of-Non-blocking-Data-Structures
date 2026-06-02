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
        
        Node(int key) : val(key), left(nullptr), right(nullptr) {}
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
    
    bool helpMark(Node* node, std::atomic<Node*>& field) {
        Node* expected = get_unmarked_ref(node);
        Node* desired = get_marked_ref(expected);
        return field.compare_exchange_strong(expected, desired,
                                            std::memory_order_acq_rel,
                                            std::memory_order_acquire);
    }
    
    void helpRemove(Node* parent, Node* target, std::atomic<Node*>& parentField) {
        Node* unmarkedTarget = get_unmarked_ref(target);
        Node* targetLeft = unmarkedTarget->left.load(std::memory_order_acquire);
        Node* targetRight = unmarkedTarget->right.load(std::memory_order_acquire);
        
        if (!targetLeft || is_marked_ref(targetLeft)) {
            Node* newChild = nullptr;
            if (targetRight && !is_marked_ref(targetRight)) {
                newChild = targetRight;
            }
            Node* expected = target;
            parentField.compare_exchange_strong(expected, newChild,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
        } else if (!targetRight || is_marked_ref(targetRight)) {
            Node* newChild = nullptr;
            if (targetLeft && !is_marked_ref(targetLeft)) {
                newChild = targetLeft;
            }
            Node* expected = target;
            parentField.compare_exchange_strong(expected, newChild,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
        } else {
            Node* successor = targetRight;
            Node* succParent = unmarkedTarget;
            
            while (successor) {
                if (is_marked_ref(successor)) {
                    break;
                }
                Node* unmarkedSucc = get_unmarked_ref(successor);
                Node* succLeft = unmarkedSucc->left.load(std::memory_order_acquire);
                
                if (!succLeft || is_marked_ref(succLeft)) {
                    break;
                }
                succParent = successor;
                successor = succLeft;
            }
            
            if (successor && !is_marked_ref(successor)) {
                Node* unmarkedSucc = get_unmarked_ref(successor);
                unmarkedTarget->val = unmarkedSucc->val;
                
                std::atomic<Node*>& succField = (successor == targetRight) ? 
                                               unmarkedTarget->right : 
                                               get_unmarked_ref(succParent)->left;
                
                Node* expectedSucc = successor;
                Node* markedSucc = get_marked_ref(get_unmarked_ref(successor));
                if (succField.compare_exchange_strong(expectedSucc, markedSucc,
                                                     std::memory_order_acq_rel,
                                                     std::memory_order_acquire)) {
                    // Node has been marked
                    helpRemove(succParent, successor, succField);
                }
            }
        }
    }
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_relaxed);
        if (r) {
            Node* unmarked = get_unmarked_ref(r);
            delete unmarked;
        }
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        
        while (current) {
            if (is_marked_ref(current)) {
                current = root.load(std::memory_order_acquire);
                continue;
            }
            
            Node* unmarked = get_unmarked_ref(current);
            int currentVal = unmarked->val;
            
            if (key == currentVal) {
                return true;
            } else if (key < currentVal) {
                current = unmarked->left.load(std::memory_order_acquire);
            } else {
                current = unmarked->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* current = root.load(std::memory_order_acquire);
            std::atomic<Node*>* parentField = &root;
            
            while (current) {
                if (is_marked_ref(current)) {
                    current = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parentField = &root;
                    continue;
                }
                
                Node* unmarked = get_unmarked_ref(current);
                int currentVal = unmarked->val;
                
                if (key == currentVal) {
                    return false;
                }
                
                parent = current;
                if (key < currentVal) {
                    parentField = &unmarked->left;
                    current = unmarked->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarked->right;
                    current = unmarked->right.load(std::memory_order_acquire);
                }
            }
            
            Node* newNode = new Node(key);
            Node* expected = nullptr;
            if (parentField->compare_exchange_strong(expected, newNode,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                return true;
            }
            delete newNode;
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* parent = nullptr;
            Node* current = root.load(std::memory_order_acquire);
            std::atomic<Node*>* parentField = &root;
            
            while (current) {
                if (is_marked_ref(current)) {
                    current = root.load(std::memory_order_acquire);
                    parent = nullptr;
                    parentField = &root;
                    continue;
                }
                
                Node* unmarked = get_unmarked_ref(current);
                int currentVal = unmarked->val;
                
                if (key == currentVal) {
                    Node* expected = current;
                    std::atomic<Node*>& targetField = parent ? 
                        ((current == get_unmarked_ref(parent)->left.load(std::memory_order_acquire)) ? 
                         get_unmarked_ref(parent)->left : get_unmarked_ref(parent)->right) : root;
                    
                    if (helpMark(current, targetField)) {
                        // Node has been marked
                        if (parent) {
                            Node* unmarkedParent = get_unmarked_ref(parent);
                            helpRemove(unmarkedParent, current, targetField);
                        } else {
                            helpRemove(nullptr, current, root);
                        }
                        return true;
                    }
                    continue;
                }
                
                parent = current;
                if (key < currentVal) {
                    parentField = &unmarked->left;
                    current = unmarked->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarked->right;
                    current = unmarked->right.load(std::memory_order_acquire);
                }
            }
            return false;
        }
    }
};