#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 16;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v) : val(v), next(nullptr) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static std::size_t hash(int key) {
        return (static_cast<std::size_t>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    // Returns true if key is found; sets pred and curr to predecessor and current node (both unmarked)
    bool find(int key, Node* head, Node** pred, Node** curr) {
        retry:
        *pred = head;
        Node* next = (*pred)->next.load(std::memory_order_acquire);
        while (true) {
            Node* succ = get_unmarked_ref(next);
            // If next is marked, help physically remove it
            if (is_marked_ref(next)) {
                Node* unmarkedNext = get_unmarked_ref(next);
                Node* nextNext = unmarkedNext->next.load(std::memory_order_acquire);
                if ((*pred)->next.compare_exchange_strong(next, get_marked_ref(nextNext),
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                    // Successfully removed marked node; retry from same pred
                    next = (*pred)->next.load(std::memory_order_acquire);
                    continue;
                } else {
                    // CAS failed, restart search
                    goto retry;
                }
            }
            // Now next is unmarked (or null)
            if (succ == nullptr || succ->val >= key) {
                *curr = succ;
                return (succ != nullptr && succ->val == key && !is_marked_ref((*pred)->next.load()));
            }
            *pred = succ;
            next = succ->next.load(std::memory_order_acquire);
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* minNode = new Node(INT_MIN);
            Node* maxNode = new Node(INT_MAX);
            minNode->next.store(maxNode, std::memory_order_relaxed);
            buckets[i].store(minNode, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* head = buckets[i].load(std::memory_order_acquire);
            Node* curr = get_unmarked_ref(head);
            while (curr != nullptr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t idx = hash(key);
        Node* head = buckets[idx].load(std::memory_order_acquire);
        Node* pred = nullptr;
        Node* curr = nullptr;
        return find(key, head, &pred, &curr);
    }

    bool add(int key) override {
        while (true) {
            std::size_t idx = hash(key);
            Node* head = buckets[idx].load(std::memory_order_acquire);
            Node* pred = nullptr;
            Node* curr = nullptr;
            bool found = find(key, head, &pred, &curr);
            if (found) {
                return false; // already present
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if ((*pred)->next.compare_exchange_strong(curr, node,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                return true; // inserted successfully
            }
            // CAS failed, retry
            delete node; // clean up allocated node
        }
    }

    bool remove(int key) override {
        while (true) {
            std::size_t idx = hash(key);
            Node* head = buckets[idx].load(std::memory_order_acquire);
            Node* pred = nullptr;
            Node* curr = nullptr;
            bool found = find(key, head, &pred, &curr);
            if (!found || curr == nullptr) {
                return false; // not present
            }
            // Logically remove curr by marking its next pointer
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (!curr->next.compare_exchange_strong(succ, get_marked_ref(succ),
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                // Failed to mark, retry
                continue;
            }
            // Physically unlink curr
            if ((*pred)->next.compare_exchange_strong(curr, succ,
                                                      std::memory_order_acq_rel,
                                                      std::memory_order_acquire)) {
                delete curr;
                return true;
            }
            // Failed to unlink, retry (help will happen in subsequent finds)
        }
    }

private:
    std::atomic<Node*> buckets[BUCKET_COUNT];
};