#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr size_t BUCKET_COUNT = 16;

    struct Node {
        int val;
        std::atomic<Node*> next;
        Node(int v, Node* n) : val(v), next(n) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1ULL);
    }

    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1ULL);
    }

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1ULL) != 0;
    }

    static size_t hash(int key) {
        return (static_cast<unsigned int>(key) & 0x7FFFFFFF) % BUCKET_COUNT;
    }

    // Returns the next unmarked node after pred, helping to remove any marked nodes found.
    Node* getNext(Node* pred) {
        Node* next = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (is_marked_ref(pred->next.load(std::memory_order_acquire))) {
            Node* succ = get_unmarked_ref(next->next.load(std::memory_order_acquire));
            pred->next.compare_exchange_strong(next, succ, std::memory_order_acq_rel, std::memory_order_relaxed);
            next = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        }
        return next;
    }

public:
    ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* minSentinel = new Node{INT_MIN, nullptr};
            Node* maxSentinel = new Node{INT_MAX, nullptr};
            minSentinel->next.store(maxSentinel, std::memory_order_release);
            buckets[i].store(minSentinel, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() {
        for (size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = buckets[i].load(std::memory_order_acquire);
            while (cur != nullptr) {
                Node* next = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = next;
            }
        }
    }

    bool contains(int key) override {
        size_t idx = hash(key);
        Node* prev = buckets[idx].load(std::memory_order_acquire);
        Node* curr = getNext(prev);
        while (curr != nullptr && curr->val < key) {
            prev = curr;
            curr = getNext(prev);
        }
        return (curr != nullptr && curr->val == key && !is_marked_ref(curr->next.load()));
    }

    bool add(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* prev = buckets[idx].load(std::memory_order_acquire);
            Node* curr = getNext(prev);
            while (curr != nullptr && curr->val < key) {
                prev = curr;
                curr = getNext(prev);
            }
            if (curr != nullptr && curr->val == key && !is_marked_ref(curr->next.load())) {
                return false;
            }
            Node* newNode = new Node{key, nullptr};
            newNode->next.store(curr, std::memory_order_release);
            if (prev->next.compare_exchange_strong(curr, newNode, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return true;
            }
            delete newNode;
            // retry if CAS failed
        }
    }

    bool remove(int key) override {
        size_t idx = hash(key);
        while (true) {
            Node* prev = buckets[idx].load(std::memory_order_acquire);
            Node* curr = getNext(prev);
            while (curr != nullptr && curr->val < key) {
                prev = curr;
                curr = getNext(prev);
            }
            if (curr == nullptr || curr->val != key) {
                return false;
            }
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* markedSucc = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, markedSucc, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                continue; // failed to mark, retry
            }
            if (prev->next.compare_exchange_strong(curr, succ, std::memory_order_acq_rel, std::memory_order_relaxed)) {
                delete curr;
                return true;
            }
            // failed to unlink, retry
        }
    }
};