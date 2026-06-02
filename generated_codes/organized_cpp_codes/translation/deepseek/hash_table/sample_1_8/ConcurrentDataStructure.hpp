#pragma once
#include "../utils/SetADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <cstddef>

class ConcurrentDataStructure : public SetADT {
private:
    static constexpr std::size_t BUCKET_COUNT = 64;
    static constexpr int HASH_MULTIPLIER = 31;

    struct Node {
        int key;
        std::atomic<Node*> next;
        
        explicit Node(int k) : key(k), next(nullptr) {}
    };

    std::atomic<Node*> buckets[BUCKET_COUNT];

    static bool is_marked_ref(Node* ptr) {
        return (reinterpret_cast<uintptr_t>(ptr) & 1) != 0;
    }

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | 1);
    }

    std::size_t hash(int key) const {
        return static_cast<std::size_t>((key * HASH_MULTIPLIER) % BUCKET_COUNT);
    }

    bool listContains(Node* head, int key) const {
        Node* curr = head;
        while (curr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            
            if (curr->key == key && !is_marked_ref(next)) {
                return true;
            }
            
            if (curr->key > key) {
                return false;
            }
            
            curr = get_unmarked_ref(next);
        }
        return false;
    }

    bool listAdd(std::atomic<Node*>& head, int key) {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred;
            
            while (true) {
                if (!curr) {
                    Node* newNode = new Node(key);
                    if (head.compare_exchange_strong(pred, newNode, std::memory_order_acq_rel)) {
                        return true;
                    }
                    delete newNode;
                    break;
                }
                
                Node* next = curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(next)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!head.compare_exchange_strong(pred, unmarked_next, std::memory_order_acq_rel)) {
                        break;
                    }
                    pred = unmarked_next;
                    curr = unmarked_next;
                    continue;
                }
                
                if (curr->key == key) {
                    return false;
                }
                
                if (curr->key > key) {
                    Node* newNode = new Node(key);
                    newNode->next.store(curr, std::memory_order_release);
                    
                    if (pred == head.load(std::memory_order_acquire)) {
                        if (head.compare_exchange_strong(pred, newNode, std::memory_order_acq_rel)) {
                            return true;
                        }
                    } else {
                        Node* pred_next = pred->next.load(std::memory_order_acquire);
                        if (!is_marked_ref(pred_next) && pred->key < key) {
                            if (pred->next.compare_exchange_strong(pred_next, newNode, std::memory_order_acq_rel)) {
                                return true;
                            }
                        }
                    }
                    delete newNode;
                    break;
                }
                
                pred = curr;
                curr = get_unmarked_ref(next);
            }
        }
    }

    bool listRemove(std::atomic<Node*>& head, int key) {
        while (true) {
            Node* pred = head.load(std::memory_order_acquire);
            Node* curr = pred;
            
            while (true) {
                if (!curr) {
                    return false;
                }
                
                Node* next = curr->next.load(std::memory_order_acquire);
                
                if (is_marked_ref(next)) {
                    Node* unmarked_next = get_unmarked_ref(next);
                    if (!head.compare_exchange_strong(pred, unmarked_next, std::memory_order_acq_rel)) {
                        break;
                    }
                    pred = unmarked_next;
                    curr = unmarked_next;
                    continue;
                }
                
                if (curr->key == key) {
                    Node* marked_next = get_marked_ref(next);
                    if (curr->next.compare_exchange_strong(next, marked_next, std::memory_order_acq_rel)) {
                        // Node has been marked
                        Node* pred_next = pred->next.load(std::memory_order_acquire);
                        if (pred == head.load(std::memory_order_acquire)) {
                            head.compare_exchange_strong(pred, get_unmarked_ref(next), std::memory_order_acq_rel);
                        } else if (!is_marked_ref(pred_next)) {
                            pred->next.compare_exchange_strong(pred_next, get_unmarked_ref(next), std::memory_order_acq_rel);
                        }
                        return true;
                    }
                    continue;
                }
                
                if (curr->key > key) {
                    return false;
                }
                
                pred = curr;
                curr = get_unmarked_ref(next);
            }
        }
    }

public:
    ConcurrentDataStructure() {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            buckets[i].store(nullptr, std::memory_order_release);
        }
    }

    ~ConcurrentDataStructure() override {
        for (std::size_t i = 0; i < BUCKET_COUNT; ++i) {
            Node* curr = buckets[i].load(std::memory_order_acquire);
            while (curr) {
                Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                delete curr;
                curr = next;
            }
        }
    }

    bool contains(int key) override {
        std::size_t index = hash(key);
        Node* head = buckets[index].load(std::memory_order_acquire);
        return listContains(head, key);
    }

    bool add(int key) override {
        std::size_t index = hash(key);
        return listAdd(buckets[index], key);
    }

    bool remove(int key) override {
        std::size_t index = hash(key);
        return listRemove(buckets[index], key);
    }
};