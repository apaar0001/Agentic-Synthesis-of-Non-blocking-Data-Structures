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
        Node(int k, Node* n) : key(k), next(n) {}
    };

    static Node* get_unmarked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) & ~static_cast<uintptr_t>(1));
    }

    static Node* get_marked_ref(Node* ptr) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(ptr) | static_cast<uintptr_t>(1));
    }

    static bool is_marked_ref(Node* ptr) {
        return reinterpret_cast<uintptr_t>(ptr) & static_cast<uintptr_t>(1);
    }

    void find(int key, Node** pred, Node** curr) {
        retry:
        *pred = head;
        *curr = get_unmarked_ref((*pred)->next.load(std::memory_order_acquire));
        while (true) {
            // Ensure curr is unmarked (if we got a marked pointer, get unmarked)
            *curr = get_unmarked_ref(*curr);
            if (*curr == tail) {
                // Validate pred->next points to curr and is unmarked
                Node* predNextRaw = (*pred)->next.load(std::memory_order_acquire);
                if (get_unmarked_ref(predNextRaw) == *curr && !is_marked_ref(predNextRaw)) {
                    return;
                }
                goto retry;
            }
            // Skip over any marked nodes after curr
            Node* succRaw = (*curr)->next.load(std::memory_order_acquire);
            while (is_marked_ref(succRaw)) {
                Node* succ = get_unmarked_ref(succRaw);
                Node* succNextRaw = succ->next.load(std::memory_order_acquire);
                Node* succNext = get_unmarked_ref(succNextRaw);
                if ((*curr)->next.compare_exchange_strong(succRaw, succNext,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_acquire)) {
                    succRaw = (*curr)->next.load(std::memory_order_acquire);
                } else {
                    goto retry;
                }
            }
            Node* succ = get_unmarked_ref(succRaw);
            if (succ->key >= key) {
                // Validate pred->next points to curr and unmarked, and curr->next points to succ and unmarked
                Node* predNextRaw = (*pred)->next.load(std::memory_order_acquire);
                Node* currNextRaw = (*curr)->next.load(std::memory_order_acquire);
                if (get_unmarked_ref(predNextRaw) == *curr && !is_marked_ref(predNextRaw) &&
                    get_unmarked_ref(currNextRaw) == succ && !is_marked_ref(currNextRaw)) {
                    return;
                }
                goto retry;
            }
            *pred = *curr;
            *curr = succ;
        }
    }

    Node* head;
    Node* tail;

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, tail);
        tail = new Node(INT_MAX, nullptr);
        head->next.store(tail, std::memory_order_relaxed);
    }

    virtual ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr != nullptr) {
            Node* next = curr->next.load(std::memory_order_acquire);
            Node* unmarked = get_unmarked_ref(next);
            delete curr;
            curr = unmarked;
        }
    }

    virtual bool contains(int key) override {
        Node* pred;
        Node* curr;
        find(key, &pred, &curr);
        if (curr == tail) return false;
        return (curr->key == key);
    }

    virtual bool add(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr != tail && curr->key == key) {
                return false;
            }
            Node* node = new Node(key, curr);
            Node* predNext = pred->next.load(std::memory_order_acquire);
            if (get_unmarked_ref(predNext) != curr || is_marked_ref(predNext)) {
                continue;
            }
            if (pred->next.compare_exchange_strong(predNext, node,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_acquire)) {
                return true;
            }
        }
    }

    virtual bool remove(int key) override {
        while (true) {
            Node* pred;
            Node* curr;
            find(key, &pred, &curr);
            if (curr == tail || curr->key != key) {
                return false;
            }
            Node* predNext = pred->next.load(std::memory_order_acquire);
            if (get_unmarked_ref(predNext) != curr || is_marked_ref(predNext)) {
                continue;
            }
            Node* markedCurr = get_marked_ref(curr);
            if (!pred->next.compare_exchange_strong(predNext, markedCurr,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                continue;
            }
            // Node has been marked
            // Help physical removal (optional but good practice)
            pred->next.compare_exchange_strong(markedCurr,
                                               curr->next.load(std::memory_order_acquire),
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
            return true;
        }
    }
};