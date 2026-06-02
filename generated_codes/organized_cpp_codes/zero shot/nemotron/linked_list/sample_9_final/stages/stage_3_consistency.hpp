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

    void find(int key, Node*& pred, Node*& curr) {
        restart:
        pred = head;
        curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
        while (true) {
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            while (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                bool snip = pred->next.compare_exchange_strong(curr, succ,
                                                               std::memory_order_acq_rel,
                                                               std::memory_order_acquire);
                if (!snip) goto restart;
                curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
                if (curr == succ) break;
                succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (!curr || curr->val >= key) {
                return;
            }
            pred = curr;
            curr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        }
    }

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN);
        Node* tail = new Node(INT_MAX);
        head->next.store(tail, std::memory_order_release);
    }

    ~ConcurrentDataStructure() {
        Node* curr = head;
        while (curr) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
    }

    bool contains(int key) {
        Node* curr = head;
        while (true) {
            Node* uncurr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (!uncurr) break;
            while (is_marked_ref(uncurr->next.load(std::memory_order_acquire))) {
                Node* succ = get_unmarked_ref(uncurr->next.load(std::memory_order_acquire));
                curr->next.compare_exchange_strong(uncurr, succ,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire);
                uncurr = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            }
            if (uncurr->val >= key) break;
            curr = uncurr;
        }
        Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
        return (succ && !is_marked_ref(succ->next.load(std::memory_order_acquire)) && succ->val == key);
    }

    bool add(int key) {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key);
            node->next.store(succ, std::memory_order_release);
            if (pred->next.compare_exchange_strong(curr, node,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire)) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) {
        while (true) {
            Node* pred = nullptr;
            Node* curr = nullptr;
            find(key, pred, curr);
            Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            if (curr->val != key) {
                return false;
            }
            Node* markedSucc = get_marked_ref(succ);
            if (!curr->next.compare_exchange_strong(succ, markedSucc,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_acquire)) {
                continue;
            }
            pred->next.compare_exchange_strong(curr, succ,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
            return true;
        }
    }
};