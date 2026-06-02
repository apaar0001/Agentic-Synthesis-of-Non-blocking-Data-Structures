#pragma once

#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class LockFreeList : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v, Node* n = nullptr) : val(v), next(n) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* search(int val, Node** left_node) {
        Node *left_node_next, *right_node;
        
    search_again:
        do {
            Node* t = head;
            Node* t_next = head->next.load(std::memory_order_acquire);
            
            // Find left_node and right_node
            do {
                if (!is_marked_ref(t_next)) {
                    *left_node = t;
                    left_node_next = t_next;
                }
                t = get_unmarked_ref(t_next);
                if (!t->next.load(std::memory_order_relaxed)) break;
                t_next = t->next.load(std::memory_order_acquire);
            } while (is_marked_ref(t_next) || (t->val < val));
            
            right_node = t;

            if (left_node_next == right_node) {
                if (right_node->next.load(std::memory_order_relaxed) && is_marked_ref(right_node->next.load(std::memory_order_relaxed)))
                    goto search_again;
                else return right_node;
            }

            // Remove one or more marked nodes
            if ((*left_node)->next.compare_exchange_strong(left_node_next, right_node, std::memory_order_acq_rel)) {
                if (right_node->next.load(std::memory_order_relaxed) && is_marked_ref(right_node->next.load(std::memory_order_relaxed)))
                    goto search_again;
                else return right_node;
            }
        } while (true);
    }

public:
    LockFreeList() {
        Node* max = new Node(INT_MAX, nullptr);
        Node* min = new Node(INT_MIN, max);
        head = min;
    }

    ~LockFreeList() override {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* curr = head;
        while (curr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(next);
            if (curr && curr->val >= key) break;
        }
        return (curr && curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_relaxed)));
    }

    bool add(int key) override {
        Node *newnode, *right_node, *left_node;
        left_node = head;
        
        do {
            right_node = search(key, &left_node);
            if (right_node->val == key)
                return false;
            
            newnode = new Node(key, right_node);
            Node* expected = right_node;
            if (left_node->next.compare_exchange_strong(expected, newnode, std::memory_order_acq_rel))
                return true;
            delete newnode;
        } while(true);
    }

    bool remove(int key) override {
        Node *right_node, *right_node_next, *left_node;
        left_node = head;
        
        do {
            right_node = search(key, &left_node);
            if (right_node->val != key)
                return false;
            
            right_node_next = right_node->next.load(std::memory_order_acquire);
            if (!is_marked_ref(right_node_next)) {
                Node* marked = get_marked_ref(right_node_next);
                if (right_node->next.compare_exchange_strong(right_node_next, marked, std::memory_order_acq_rel))
                    break;
            }
        } while(true);
        
        Node* expected = right_node;
        if (!left_node->next.compare_exchange_strong(expected, right_node_next, std::memory_order_acq_rel)) {
            right_node = search(right_node->val, &left_node);
        }
        return true;
    }
};
