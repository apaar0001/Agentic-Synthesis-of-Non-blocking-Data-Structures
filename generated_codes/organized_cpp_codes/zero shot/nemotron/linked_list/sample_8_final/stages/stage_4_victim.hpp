#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    Node* head;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1UL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1UL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1UL);
    }

    void find(int key, Node*& pred, Node*& curr) {
        pred = head;
        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (curr->val < key) {
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                pred->next.compare_exchange_weak(curr, succ,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire);
                curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            pred = curr;
            curr = succ;
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
    }

    bool contains(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, pred, curr);
        return (curr && curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire)));
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (curr && curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_weak(curr, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                                                     std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            if (!curr || curr->val != key) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                continue;
            }
            pred->next.compare_exchange_weak(curr, succ,
                                             std::memory_order_acq_rel,
                                             std::memory_order_acquire);
            delete curr;
            return true;
        }
    }
};