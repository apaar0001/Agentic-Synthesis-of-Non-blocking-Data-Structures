#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    void find(int key, Node** pred, Node** curr) {
        retry:
        while (true) {
            *pred = head;
            Node* currNode = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(currNode->next.load(std::memory_order_acquire));
                while (is_marked_ref(currNode->next.load(std::memory_order_acquire))) {
                    Node* succNext = get_unmarked_ref(succ->next.load(std::memory_order_acquire));
                    if (!(*pred)->next.compare_exchange_strong(currNode, succNext,
                            std::memory_order_acq_rel, std::memory_order_acquire)) {
                        goto retry;
                    }
                    currNode = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
                    succ = get_unmarked_ref(currNode->next.load(std::memory_order_acquire));
                }
                if (currNode->val >= key) {
                    *curr = currNode;
                    return;
                }
                *pred = currNode;
                currNode = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
    }

    bool contains(int key) override {
        Node* pred;
        Node* curr;
        find(key, &pred, &curr);
        return (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_strong(curr, node,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr->val != key) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                continue;
            }
            pred->next.compare_exchange_strong(curr, succ,
                    std::memory_order_acq_rel, std::memory_order_acquire);
            delete curr;
            return true;
        }
    }
};