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
        Node* grandparent;
    };

    SearchResult search(int key, Node* start) {
        SearchResult result{nullptr, nullptr, nullptr};
        Node* parent = nullptr;
        Node* current = start;
        Node* gp = nullptr;
        
        while (current != nullptr) {
            Node* unmarked = get_unmarked_ref(current);
            if (unmarked == nullptr) break;
            
            if (is_marked_ref(current->left.load(std::memory_order_acquire)) ||
                is_marked_ref(current->right.load(std::memory_order_acquire))) {
                if (parent != nullptr) {
                    Node* next = (key < parent->val) ? parent->left.load(std::memory_order_acquire) 
                                                     : parent->right.load(std::memory_order_acquire);
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (unmarked_next == unmarked) {
                        Node* marked = get_marked_ref(unmarked);
                        if (key < parent->val) {
                            parent->left.compare_exchange_strong(next, marked, 
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        } else {
                            parent->right.compare_exchange_strong(next, marked, 
                                std::memory_order_acq_rel, std::memory_order_acquire);
                        }
                    }
                }
                current = start;
                parent = nullptr;
                gp = nullptr;
                continue;
            }
            
            if (unmarked->val == key) {
                result.parent = parent;
                result.target = unmarked;
                result.grandparent = gp;
                return result;
            }
            
            gp = parent;
            parent = unmarked;
            current = (key < unmarked->val) ? unmarked->left.load(std::memory_order_acquire) 
                                            : unmarked->right.load(std::memory_order_acquire);
        }
        
        result.parent = parent;
        result.target = nullptr;
        result.grandparent = gp;
        return result;
    }

    void cleanup(Node* node) {
        if (node == nullptr) return;
        Node* left = get_unmarked_ref(node->left.load(std::memory_order_acquire));
        Node* right = get_unmarked_ref(node->right.load(std::memory_order_acquire));
        cleanup(left);
        cleanup(right);
        delete node;
    }

public:
    ConcurrentDataStructure() {
        Node* r = new Node(INT_MAX);
        r->left.store(new Node(INT_MIN), std::memory_order_release);
        root.store(r, std::memory_order_release);
    }

    ~ConcurrentDataStructure() override {
        Node* r = root.load(std::memory_order_acquire);
        cleanup(r);
    }

    bool contains(int key) override {
        Node* current = root.load(std::memory_order_acquire);
        while (current != nullptr) {
            Node* unmarked = get_unmarked_ref(current);
            if (unmarked == nullptr) break;
            
            if (unmarked->val == key) {
                return !is_marked_ref(current->left.load(std::memory_order_acquire)) &&
                       !is_marked_ref(current->right.load(std::memory_order_acquire));
            }
            
            current = (key < unmarked->val) ? unmarked->left.load(std::memory_order_acquire) 
                                            : unmarked->right.load(std::memory_order_acquire);
        }
        return false;
    }

    bool add(int key) override {
        while (true) {
            Node* r = root.load(std::memory_order_acquire);
            SearchResult res = search(key, r);
            
            if (res.target != nullptr) {
                return false;
            }
            
            Node* newNode = new Node(key);
            Node* parent = res.parent;
            
            if (parent == nullptr) {
                if (root.compare_exchange_strong(r, newNode, 
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                delete newNode;
                continue;
            }
            
            std::atomic<Node*>* childPtr = (key < parent->val) ? &parent->left : &parent->right;
            Node* expected = childPtr->load(std::memory_order_acquire);
            
            if (is_marked_ref(expected)) {
                delete newNode;
                continue;
            }
            
            newNode->left.store(expected, std::memory_order_release);
            if (childPtr->compare_exchange_strong(expected, newNode, 
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            
            delete newNode;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* r = root.load(std::memory_order_acquire);
            SearchResult res = search(key, r);
            
            if (res.target == nullptr) {
                return false;
            }
            
            Node* target = res.target;
            Node* parent = res.parent;
            
            if (parent == nullptr) {
                Node* marked = get_marked_ref(target);
                if (root.compare_exchange_strong(target, marked, 
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                        std::this_thread::sleep_for(std::chrono::seconds(5));
                    return true;
                }
                continue;
            }
            
            std::atomic<Node*>* childPtr = (key < parent->val) ? &parent->left : &parent->right;
            Node* expected = childPtr->load(std::memory_order_acquire);
            
            if (get_unmarked_ref(expected) != target || is_marked_ref(expected)) {
                continue;
            }
            
            Node* marked = get_marked_ref(target);
            if (childPtr->compare_exchange_strong(expected, marked, 
                std::memory_order_acq_rel, std::memory_order_acquire)) {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
        }
    }
};