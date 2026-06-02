#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <array>
#include <thread>
#include <chrono>

class ConcurrentDataStructure : public StackADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static constexpr int ELIM_SIZE = 8;
    static constexpr int ELIM_SPIN = 256;
    static constexpr int EMPTY  = 0;
    static constexpr int PUSH   = 1;
    static constexpr int POP    = 2;
    static constexpr int DONE   = 3;

    struct Slot {
        std::atomic<int>  state{EMPTY};
        std::atomic<int>  val{0};
    };

    std::atomic<Node*> top{nullptr};
    std::array<Slot, ELIM_SIZE> elim_array;

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
    }

    bool try_push_central(Node* node) {
        Node* old = top.load(std::memory_order_acquire);
        node->next.store(get_unmarked_ref(old), std::memory_order_relaxed);
        return top.compare_exchange_strong(old, node,
            std::memory_order_acq_rel, std::memory_order_acquire);
    }

    bool try_pop_central(int& out) {
        Node* t = get_unmarked_ref(top.load(std::memory_order_acquire));
        if (!t) return false;
        Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
        if (top.compare_exchange_strong(t, next,
                std::memory_order_acq_rel, std::memory_order_acquire)) {
            out = t->val;
            delete t;
            return true;
        }
        return false;
    }

public:
    ConcurrentDataStructure() = default;

    ~ConcurrentDataStructure() override {
        Node* cur = get_unmarked_ref(top.load(std::memory_order_relaxed));
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = nxt;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);
        // Try central stack first
        for (int attempt = 0; attempt < 3; ++attempt)
            if (try_push_central(node)) return;

        // Try elimination
        int slot_idx = std::hash<std::thread::id>{}(std::this_thread::get_id()) % ELIM_SIZE;
        Slot& slot = elim_array[slot_idx];
        int exp_empty = EMPTY;
        if (slot.state.compare_exchange_strong(exp_empty, PUSH,
                std::memory_order_acq_rel, std::memory_order_relaxed)) {
            slot.val.store(val, std::memory_order_release);
            for (int spin = 0; spin < ELIM_SPIN; ++spin) {
                if (slot.state.load(std::memory_order_acquire) == DONE) {
                    slot.state.store(EMPTY, std::memory_order_release);
                    delete node;
                    return;
                }
            }
            // Elimination failed, reset and retry central
            int push_st = PUSH;
            if (!slot.state.compare_exchange_strong(push_st, EMPTY,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                // A pop already matched — wait for it to set DONE
                while (slot.state.load(std::memory_order_acquire) != DONE) {}
                slot.state.store(EMPTY, std::memory_order_release);
                delete node;
                return;
            }
        }
        // Fall back to central
        while (!try_push_central(node)) {}
    }

    int pop() override {
        int result = INT_MIN;
        // Try central stack first
        for (int attempt = 0; attempt < 3; ++attempt)
            if (try_pop_central(result)) return result;

        // Try elimination
        int slot_idx = (std::hash<std::thread::id>{}(std::this_thread::get_id()) + 1) % ELIM_SIZE;
        Slot& slot = elim_array[slot_idx];
        for (int spin = 0; spin < ELIM_SPIN; ++spin) {
            int st = slot.state.load(std::memory_order_acquire);
            if (st == PUSH) {
                int push_st = PUSH;
                if (slot.state.compare_exchange_strong(push_st, DONE,
                        std::memory_order_acq_rel, std::memory_order_relaxed))
                    return slot.val.load(std::memory_order_acquire);
            }
        }
        // Fall back to central
        while (true) {
            Node* t = get_unmarked_ref(top.load(std::memory_order_acquire));
            if (!t) return INT_MIN;
            if (try_pop_central(result)) return result;
        }
    }

    bool isEmpty() override {
        return get_unmarked_ref(top.load(std::memory_order_acquire)) == nullptr;
    }
};
