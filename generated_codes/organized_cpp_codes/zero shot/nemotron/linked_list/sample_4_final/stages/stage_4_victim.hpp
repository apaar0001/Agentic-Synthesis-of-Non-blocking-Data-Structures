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
        Node(int v, Node* n) : val(v), next(n) {}
    };

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1LU) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1LU);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1LU);
    }

    Node* head;
    Node* tail;

public:
    ConcurrentDataStructure() {
        head = new Node(INT_MIN, nullptr);
        tail = new Node(INT_MAX, nullptr);
        head->next.store(tail, std::memory_order_relaxed);
        tail->next.store(nullptr, std::memory_order_relaxed);
    }

    ~ConcurrentDataStructure() {
        Node* curr = get_unmarked_ref(head->next.load(std::memory_order_acquire));
        while (curr != tail) {
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            delete curr;
            curr = next;
        }
        delete head;
        delete tail;
    }

    bool contains(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* nextUnmarked = get_unmarked_ref(succ);
                    pred->next.compare_exchange_weak(curr, nextUnmarked,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_relaxed);
                    continue;
                }
                if (curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (curr->val == key && !is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return true;
            }
            return false;
        }
    }

    bool add(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* nextUnmarked = get_unmarked_ref(succ);
                    pred->next.compare_exchange_weak(curr, nextUnmarked,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_relaxed);
                    continue;
                }
                if (curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (curr->val == key) {
                return false;
            }
            Node* node = new Node(key, nullptr);
            node->next.store(curr, std::memory_order_relaxed);
            if (pred->next.compare_exchange_weak(curr, node,
                                                std::memory_order_acq_rel,
                                                std::memory_order_relaxed)) {
                                                    std::this_thread::sleep_for(std::chrono::seconds(5));
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) override {
        while (true) {
            Node* pred = head;
            Node* curr = get_unmarked_ref(pred->next.load(std::memory_order_acquire));
            while (true) {
                Node* succ = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
                if (is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                    Node* nextUnmarked = get_unmarked_ref(succ);
                    pred->next.compare_exchange_weak(curr, nextUnmarked,
                                                    std::memory_order_acq_rel,
                                                    std::memory_order_relaxed);
                    continue;
                }
                if (curr->val >= key) {
                    break;
                }
                pred = curr;
                curr = succ;
            }
            if (curr->val != key || is_marked_ref(curr->next.load(std::memory_order_acquire))) {
                return false;
            }
            Node* next = get_unmarked_ref(curr->next.load(std::memory_order_acquire));
            Node* markedNext = get_marked_ref(next);
            if (!curr->next.compare_exchange_weak(next, markedNext,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed)) {
                continue;
            }
            pred->next.compare_exchange_weak(curr, next,
                                            std::memory_order_acq_rel,
                                            std::memory_order_relaxed);
            delete curr;
            return true;
        }
    }
};