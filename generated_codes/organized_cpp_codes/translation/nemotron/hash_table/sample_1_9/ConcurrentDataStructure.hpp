#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <cstdint>
#include <climits>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr unsigned int BUCKET_COUNT = 64;
    struct Node {
        int key;
        std::atomic<Node*> next;
        Node(int k) : key(k) { next.store(nullptr, std::memory_order_relaxed); }
    };
    std::atomic<Node*> heads[BUCKET_COUNT];

    static Node* get_unmarked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    unsigned int hash(int key) const {
        return static_cast<unsigned int>(key >= 0 ? key : -key) % BUCKET_COUNT;
    }

    void find(int key, unsigned h, Node*& pred, Node*& curr) {
        Node* head = heads[h].load(std::memory_order_acquire);
        retry:
        pred = head;
        while (true) {
            Node* curr_ref = pred->next.load(std::memory_order_acquire);
            curr = get_unmarked(curr_ref);
            if (is_marked(curr_ref)) {
                Node* node_ptr = get_unmarked(curr_ref);
                Node* succ = get_unmarked(node_ptr->next.load(std::memory_order_acquire));
                Node* expected = curr_ref;
                pred->next.compare_exchange_strong(expected, succ,
                                                 std::memory_order_acq_rel, std::memory_order_relaxed);
                continue;
            }
            if (curr->key >= key) {
                break;
            }
            pred = curr;
        }
        Node* curr_ref = pred->next.load(std::memory_order_acquire);
        if (is_marked(curr_ref) || get_unmarked(curr_ref) != curr) {
            goto retry;
        }
    }

public:
    ConcurrentDataStructure() {
        for (unsigned i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = new Node(INT_MIN);
            Node* tail = new Node(INT_MAX);
            head->next.store(tail, std::memory_order_relaxed);
            heads[i].store(head, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() {
        for (unsigned i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = heads[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        unsigned h = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, h, pred, curr);
        return (curr->key == key) && !is_marked(pred->next.load(std::memory_order_acquire));
    }

    bool add(int key) override {
        while (true) {
            unsigned h = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, h, pred, curr);
            if (curr->key == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            Node* expected = pred->next.load(std::memory_order_acquire);
            if (pred->next.compare_exchange_strong(expected, node,
                                                 std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
        }
    }

    bool remove(int key) override {
        while (true) {
            unsigned h = hash(key);
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, h, pred, curr);
            if (curr->key != key) {
                return false;
            }
            Node* expected = curr;
            if (pred->next.compare_exchange_strong(expected, get_marked(curr),
                                                 std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // Node has been marked
                return true;
            }
        }
    }
};