#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public SetADT {
private:
    struct Node {
        int key;
        std::atomic<Node*> next;
        explicit Node(int k) : key(k), next(nullptr) {}
    };

    static constexpr unsigned int BUCKET_COUNT = 64;
    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~1ULL);
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1ULL);
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & 1ULL;
    }

    unsigned int hash(int key) const {
        return static_cast<unsigned int>((key % static_cast<int>(BUCKET_COUNT) + BUCKET_COUNT) % BUCKET_COUNT);
    }

    bool find(int key, Node*& pred, Node*& curr) {
        unsigned idx = hash(key);
        retry:
        pred = buckets[idx].load(std::memory_order_acquire);
        Node* pred_next = pred->next.load(std::memory_order_acquire);
        curr = get_unmarked_ref(pred_next);
        while (curr) {
            // Validate pred->next hasn't changed
            Node* current_pred_next = pred->next.load(std::memory_order_acquire);
            if (current_pred_next != pred_next) {
                goto retry;
            }
            // Help remove marked nodes
            Node* curr_next = curr->next.load(std::memory_order_acquire);
            if (is_marked_ref(curr_next)) {
                Node* succ = get_unmarked_ref(curr_next);
                if (pred->next.compare_exchange_strong(pred_next,
                                                    get_marked_ref(succ),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    pred_next = pred->next.load(std::memory_order_acquire);
                    curr = get_unmarked_ref(pred_next);
                    continue;
                } else {
                    goto retry;
                }
            }
            if (curr->key >= key) {
                break;
            }
            pred = curr;
            pred_next = curr->next.load(std::memory_order_acquire);
            curr = get_unmarked_ref(pred_next);
        }
        return true;
    }

public:
    ConcurrentDataStructure() {
        for (unsigned int i = 0; i < BUCKET_COUNT; ++i) {
            Node* dummy = new Node(INT_MIN);
            dummy->next.store(nullptr, std::memory_order_release);
            buckets[i].store(dummy, std::memory_order_release);
        }
    }
    ~ConcurrentDataStructure() override {
        for (unsigned int i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = buckets[i].load(std::memory_order_acquire);
            Node* cur = head;
            while (cur) {
                Node* nxt = cur->next.load(std::memory_order_acquire);
                cur = get_unmarked_ref(nxt);
                delete head;
                head = cur;
            }
        }
    }

    bool contains(int key) override {
        Node* pred = nullptr;
        Node* curr = nullptr;
        if (!find(key, pred, curr)) return false;
        return (curr && curr->key == key);
    }

    bool add(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, pred, curr)) return false;
            if (curr && curr->key == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_release);
            if (pred->next.compare_exchange_strong(
                    /* expected */ curr,
                    node,
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, pred, curr)) return false;
            if (!curr || curr->key != key) {
                return false;
            }
            Node* curr_next = curr->next.load(std::memory_order_acquire);
            if (pred->next.compare_exchange_strong(
                    /* expected */ curr,
                    get_marked_ref(curr),
                    std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                // Node has been marked
                return true;
            }
        }
    }
};