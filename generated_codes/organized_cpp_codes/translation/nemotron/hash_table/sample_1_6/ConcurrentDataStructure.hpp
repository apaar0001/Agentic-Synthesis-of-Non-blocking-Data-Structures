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
        Node(int k) : key(k), next(nullptr) {}
    };
    std::atomic<Node*> buckets[BUCKET_COUNT];

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }
    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }
    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    unsigned int hash(int key) const {
        int bucket = key % static_cast<int>(BUCKET_COUNT);
        if (bucket < 0) bucket += static_cast<int>(BUCKET_COUNT);
        return static_cast<unsigned int>(bucket);
    }

    bool find(int key, unsigned int idx, Node*& pred, Node*& curr) {
        Node* head_ptr;
        retry_head:
        head_ptr = buckets[idx].load(std::memory_order_acquire);
        while (head_ptr && is_marked_ref(head_ptr)) {
            Node* head = get_unmarked_ref(head_ptr);
            Node* next_ptr = head->next.load(std::memory_order_acquire);
            if (buckets[idx].compare_exchange_strong(head_ptr, next_ptr,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                head_ptr = next_ptr;
            } else {
                head_ptr = buckets[idx].load(std::memory_order_acquire);
                goto retry_head;
            }
        }
        if (!head_ptr) {
            pred = nullptr;
            curr = nullptr;
            return false;
        }
        Node* head = get_unmarked_ref(head_ptr);
        pred = nullptr;
        curr = head;

        while (curr) {
            if (is_marked_ref(curr)) {
                Node* succ = curr->next.load(std::memory_order_acquire);
                Node* unmarked_succ = get_unmarked_ref(succ);
                if (pred) {
                    if (!pred->next.compare_exchange_strong(curr, unmarked_succ,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        goto retry_head;
                    }
                } else {
                    if (!buckets[idx].compare_exchange_strong(curr, unmarked_succ,
                                                          std::memory_order_acq_rel,
                                                          std::memory_order_acquire)) {
                        goto retry_head;
                    }
                }
                goto retry_head;
            }

            Node* next_ptr = curr->next.load(std::memory_order_acquire);
            while (next_ptr && is_marked_ref(next_ptr)) {
                Node* real_next = get_unmarked_ref(next_ptr);
                Node* next_next_ptr = real_next->next.load(std::memory_order_acquire);
                if (curr->next.compare_exchange_strong(next_ptr, next_next_ptr,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    next_ptr = curr->next.load(std::memory_order_acquire);
                } else {
                    goto retry_head;
                }
            }

            if (!next_ptr || get_unmarked_ref(next_ptr)->key >= key) {
                break;
            }

            pred = curr;
            curr = get_unmarked_ref(next_ptr);
        }
        return (curr && curr->key == key);
    }

public:
    ConcurrentDataStructure() {
        for (unsigned int i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_relaxed);
        }
    }

    ~ConcurrentDataStructure() override {
        for (unsigned int i = 0; i < BUCKET_COUNT; ++i) {
            Node* cur = buckets[i].load(std::memory_order_acquire);
            while (cur) {
                Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_acquire));
                delete cur;
                cur = nxt;
            }
        }
    }

    bool contains(int key) override {
        unsigned int idx = hash(key);
        Node* pred = nullptr;
        Node* curr = nullptr;
        find(key, idx, pred, curr);
        return (curr && curr->key == key);
    }

    bool add(int key) override {
        unsigned int idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (find(key, idx, pred, curr)) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred) {
                if (pred->next.compare_exchange_strong(curr, node,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    return true;
                }
            } else {
                if (buckets[idx].compare_exchange_strong(curr, node,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                    return true;
                }
            }
            delete node;
        }
    }

    bool remove(int key) override {
        unsigned int idx = hash(key);
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            if (!find(key, idx, pred, curr)) {
                return false;
            }
            Node* next_ptr = curr->next.load(std::memory_order_acquire);
            Node* marked_next = get_marked_ref(next_ptr);
            if (pred) {
                if (!pred->next.compare_exchange_strong(next_ptr, marked_next,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    continue;
                }
            } else {
                if (!buckets[idx].compare_exchange_strong(next_ptr, marked_next,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                    continue;
                }
            }
            // Node has been marked
            Node* unmarked_next = get_unmarked_ref(next_ptr);
            if (pred) {
                if (!pred->next.compare_exchange_strong(marked_next, unmarked_next,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                    continue;
                }
            } else {
                if (!buckets[idx].compare_exchange_strong(marked_next, unmarked_next,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                    continue;
                }
            }
            return true;
        }
    }
};