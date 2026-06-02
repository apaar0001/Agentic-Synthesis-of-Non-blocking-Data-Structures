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
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    Node* find(int key, Node** pred) {
        while (true) {
            Node* prev = head;
            Node* curr = prev->next.load(std::memory_order_acquire);
            while (true) {
                Node* currUnmarked = get_unmarked_ref(curr);
                bool marked = is_marked_ref(curr);
                Node* succ = currUnmarked->next.load(std::memory_order_acquire);
                if (marked) {
                    Node* nextUnmarked = get_unmarked_ref(succ);
                    prev->next.compare_exchange_weak(curr, get_marked_ref(nextUnmarked),
                                                     std::memory_order_acq_rel, std::memory_order_acquire);
                    curr = prev->next.load(std::memory_order_acquire);
                    continue;
                }
                if (currUnmarked->val >= key) {
                    *pred = prev;
                    return currUnmarked;
                }
                prev = currUnmarked;
                curr = succ;
            }
        }
    }

public:
    ConcurrentDataStructure() {
        Node* minNode = new Node(INT_MIN);
        Node* maxNode = new Node(INT_MAX);
        minNode->next.store(maxNode, std::memory_order_relaxed);
        head = minNode;
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_relaxed));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) override {
        Node* pred;
        Node* curr = find(key, &pred);
        return (curr != nullptr && curr->val == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr = find(key, &pred);
            if (curr != nullptr && curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_weak(curr, node,
                                                 std::memory_order_acq_rel, std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr = find(key, &pred);
            if (curr == nullptr || curr->val != key) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (!curr->next.compare_exchange_weak(succ, get_marked_ref(succ),
                                                     std::memory_order_acq_rel, std::memory_order_acquire)) {
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    break;
                }
            }
            return true;
        }
    }
};