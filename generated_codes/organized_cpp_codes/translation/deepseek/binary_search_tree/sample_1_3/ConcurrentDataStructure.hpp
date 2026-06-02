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
    
public:
    ConcurrentDataStructure() : root(nullptr) {}
    
    ~ConcurrentDataStructure() override {
        Node* current = root.load(std::memory_order_relaxed);
        if (current) {
            Node* unmarked = get_unmarked_ref(current);
            delete unmarked;
        }
    }
    
    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        while (current) {
            Node* unmarkedCurrent = get_unmarked_ref(current);
            if (unmarkedCurrent->val == key) {
                return !is_marked_ref(current);
            } else if (key < unmarkedCurrent->val) {
                current = unmarkedCurrent->left.load(std::memory_order_acquire);
            } else {
                current = unmarkedCurrent->right.load(std::memory_order_acquire);
            }
        }
        return false;
    }
    
    bool add(int key) override {
        while (true) {
            Node* current = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* parentField = &root;
            
            while (true) {
                if (!current) {
                    Node* newNode = new Node(key);
                    Node* expected = nullptr;
                    if (parentField->compare_exchange_strong(expected, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                    delete newNode;
                    break;
                }
                
                Node* unmarkedCurrent = get_unmarked_ref(current);
                if (is_marked_ref(current)) {
                    Node* successor = unmarkedCurrent->right.load(std::memory_order_acquire);
                    Node* expected = current;
                    if (parentField->compare_exchange_strong(expected, successor, std::memory_order_acq_rel)) {
                        current = root.load(std::memory_order_acquire);
                        parent = nullptr;
                        parentField = &root;
                        continue;
                    }
                    break;
                }
                
                if (unmarkedCurrent->val == key) {
                    return false;
                }
                
                parent = unmarkedCurrent;
                if (key < unmarkedCurrent->val) {
                    parentField = &unmarkedCurrent->left;
                    current = unmarkedCurrent->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarkedCurrent->right;
                    current = unmarkedCurrent->right.load(std::memory_order_acquire);
                }
            }
        }
    }
    
    bool remove(int key) override {
        while (true) {
            Node* current = root.load(std::memory_order_acquire);
            Node* parent = nullptr;
            std::atomic<Node*>* parentField = &root;
            
            while (true) {
                if (!current) {
                    return false;
                }
                
                Node* unmarkedCurrent = get_unmarked_ref(current);
                if (is_marked_ref(current)) {
                    Node* successor = unmarkedCurrent->right.load(std::memory_order_acquire);
                    Node* expected = current;
                    if (parentField->compare_exchange_strong(expected, successor, std::memory_order_acq_rel)) {
                        current = root.load(std::memory_order_acquire);
                        parent = nullptr;
                        parentField = &root;
                        continue;
                    }
                    break;
                }
                
                if (unmarkedCurrent->val == key) {
                    Node* left = unmarkedCurrent->left.load(std::memory_order_acquire);
                    Node* right = unmarkedCurrent->right.load(std::memory_order_acquire);
                    
                    if (!left && !right) {
                        Node* markedNode = get_marked_ref(unmarkedCurrent);
                        Node* expected = current;
                        if (parentField->compare_exchange_strong(expected, markedNode, std::memory_order_acq_rel)) {
                            // Node has been marked
                            Node* expectedMarked = markedNode;
                            Node* desired = nullptr;
                            if (parentField->compare_exchange_strong(expectedMarked, desired, std::memory_order_acq_rel)) {
                                return true;
                            }
                        }
                        break;
                    } else if (!left) {
                        Node* markedRight = get_marked_ref(right);
                        Node* expectedRight = right;
                        if (unmarkedCurrent->right.compare_exchange_strong(expectedRight, markedRight, std::memory_order_acq_rel)) {
                            // Node has been marked
                            Node* expectedCurrent = current;
                            if (parentField->compare_exchange_strong(expectedCurrent, right, std::memory_order_acq_rel)) {
                                return true;
                            }
                        }
                        break;
                    } else if (!right) {
                        Node* markedLeft = get_marked_ref(left);
                        Node* expectedLeft = left;
                        if (unmarkedCurrent->left.compare_exchange_strong(expectedLeft, markedLeft, std::memory_order_acq_rel)) {
                            // Node has been marked
                            Node* expectedCurrent = current;
                            if (parentField->compare_exchange_strong(expectedCurrent, left, std::memory_order_acq_rel)) {
                                return true;
                            }
                        }
                        break;
                    } else {
                        Node* successorParent = unmarkedCurrent;
                        std::atomic<Node*>* successorParentField = &unmarkedCurrent->right;
                        Node* successor = right;
                        
                        while (successor) {
                            Node* unmarkedSuccessor = get_unmarked_ref(successor);
                            if (is_marked_ref(successor)) {
                                Node* succRight = unmarkedSuccessor->right.load(std::memory_order_acquire);
                                Node* expectedSuccessor = successor;
                                if (successorParentField->compare_exchange_strong(expectedSuccessor, succRight, std::memory_order_acq_rel)) {
                                    successor = successorParentField->load(std::memory_order_acquire);
                                    continue;
                                }
                                break;
                            }
                            
                            Node* succLeft = unmarkedSuccessor->left.load(std::memory_order_acquire);
                            if (!succLeft) {
                                Node* markedSuccRight = get_marked_ref(unmarkedSuccessor->right.load(std::memory_order_acquire));
                                Node* expectedSuccRight = unmarkedSuccessor->right.load(std::memory_order_acquire);
                                if (unmarkedSuccessor->right.compare_exchange_strong(expectedSuccRight, markedSuccRight, std::memory_order_acq_rel)) {
                                    // Node has been marked
                                    unmarkedCurrent->val = unmarkedSuccessor->val;
                                    Node* succRight = get_unmarked_ref(markedSuccRight);
                                    Node* expectedSuccessor = successor;
                                    if (successorParentField->compare_exchange_strong(expectedSuccessor, succRight, std::memory_order_acq_rel)) {
                                        return true;
                                    }
                                }
                                break;
                            }
                            
                            successorParent = unmarkedSuccessor;
                            successorParentField = &unmarkedSuccessor->left;
                            successor = succLeft;
                        }
                        break;
                    }
                }
                
                parent = unmarkedCurrent;
                if (key < unmarkedCurrent->val) {
                    parentField = &unmarkedCurrent->left;
                    current = unmarkedCurrent->left.load(std::memory_order_acquire);
                } else {
                    parentField = &unmarkedCurrent->right;
                    current = unmarkedCurrent->right.load(std::memory_order_acquire);
                }
            }
        }
    }
};