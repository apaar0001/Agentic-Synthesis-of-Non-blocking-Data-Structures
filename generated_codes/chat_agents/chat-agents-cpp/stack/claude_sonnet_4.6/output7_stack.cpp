#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>
#include <array>
#include <vector>
#include <algorithm>
#include <thread>

class ConcurrentDataStructure : public StackADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    static constexpr int MAX_THREADS  = 64;
    static constexpr int RETIRE_LIMIT = 16;

    // Hazard pointer table
    struct HPRecord {
        std::atomic<Node*> hp{nullptr};
        std::atomic<bool>  active{false};
    };

    static HPRecord hp_table[MAX_THREADS];

    struct ThreadLocal {
        int            id{-1};
        std::vector<Node*> retired;
    };

    static thread_local ThreadLocal tl;

    static int acquire_hp_record() {
        for (int i = 0; i < MAX_THREADS; ++i) {
            bool exp = false;
            if (hp_table[i].active.compare_exchange_strong(exp, true,
                    std::memory_order_acq_rel, std::memory_order_relaxed))
                return i;
        }
        return -1; // should not happen in well-dimensioned code
    }

    static void ensure_thread_registered() {
        if (tl.id == -1) tl.id = acquire_hp_record();
    }

    static void protect(Node* n) {
        ensure_thread_registered();
        if (tl.id >= 0) hp_table[tl.id].hp.store(n, std::memory_order_release);
    }

    static void clear_protection() {
        if (tl.id >= 0) hp_table[tl.id].hp.store(nullptr, std::memory_order_release);
    }

    static void retire(Node* n) {
        ensure_thread_registered();
        tl.retired.push_back(n);
        if ((int)tl.retired.size() >= RETIRE_LIMIT) scan_and_reclaim();
    }

    static void scan_and_reclaim() {
        // Collect all active hazard pointers
        std::vector<Node*> hazards;
        hazards.reserve(MAX_THREADS);
        for (int i = 0; i < MAX_THREADS; ++i) {
            if (hp_table[i].active.load(std::memory_order_acquire)) {
                Node* h = hp_table[i].hp.load(std::memory_order_acquire);
                if (h) hazards.push_back(h);
            }
        }
        std::sort(hazards.begin(), hazards.end());

        std::vector<Node*> still_pending;
        for (Node* n : tl.retired) {
            if (std::binary_search(hazards.begin(), hazards.end(), n))
                still_pending.push_back(n);
            else
                delete n;
        }
        tl.retired = std::move(still_pending);
    }

    std::atomic<Node*> top{nullptr};

    static bool is_marked_ref(Node* p) {
        return (reinterpret_cast<uintptr_t>(p) & 1L) != 0;
    }
    static Node* get_unmarked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) & ~1L);
    }
    static Node* get_marked_ref(Node* p) {
        return reinterpret_cast<Node*>(reinterpret_cast<uintptr_t>(p) | 1L);
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
        // Reclaim anything left in retire lists
        if (tl.id >= 0) {
            for (Node* n : tl.retired) delete n;
            tl.retired.clear();
            hp_table[tl.id].hp.store(nullptr, std::memory_order_release);
            hp_table[tl.id].active.store(false, std::memory_order_release);
            tl.id = -1;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);
        Node* old_top;
        do {
            old_top = get_unmarked_ref(top.load(std::memory_order_acquire));
            node->next.store(old_top, std::memory_order_relaxed);
        } while (!top.compare_exchange_weak(old_top, node,
                    std::memory_order_acq_rel, std::memory_order_acquire));
    }

    int pop() override {
        Node* t;
        Node* next;
        while (true) {
            do {
                t = top.load(std::memory_order_acquire);
                t = get_unmarked_ref(t);
                protect(t);
            } while (t != get_unmarked_ref(top.load(std::memory_order_acquire)));

            if (!t) { clear_protection(); return INT_MIN; }

            next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (top.compare_exchange_strong(t, next,
                    std::memory_order_acq_rel, std::memory_order_acquire)) {
                clear_protection();
                int res = t->val;
                retire(t);
                return res;
            }
        }
    }

    bool isEmpty() override {
        return get_unmarked_ref(top.load(std::memory_order_acquire)) == nullptr;
    }
};

// Out-of-class static member definitions
inline ConcurrentDataStructure::HPRecord
    ConcurrentDataStructure::hp_table[ConcurrentDataStructure::MAX_THREADS];

inline thread_local ConcurrentDataStructure::ThreadLocal
    ConcurrentDataStructure::tl;
