#pragma once
#include "../utils/StackADT.hpp"
#include <atomic>
#include <climits>
#include <cstdint>

class ConcurrentDataStructure : public StackADT {
    struct Node {
        int val;
        std::atomic<Node*> next;
        explicit Node(int v) : val(v), next(nullptr) {}
    };

    struct TaggedPtr {
        Node*    ptr;
        uint64_t tag;
        bool operator==(const TaggedPtr& o) const {
            return ptr == o.ptr && tag == o.tag;
        }
    };

    std::atomic<TaggedPtr> top;

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
    ConcurrentDataStructure() : top(TaggedPtr{nullptr, 0}) {}

    ~ConcurrentDataStructure() override {
        TaggedPtr t = top.load(std::memory_order_relaxed);
        Node* cur = get_unmarked_ref(t.ptr);
        while (cur) {
            Node* nxt = get_unmarked_ref(cur->next.load(std::memory_order_relaxed));
            delete cur;
            cur = nxt;
        }
    }

    void push(int val) override {
        Node* node = new Node(val);
        TaggedPtr old_top;
        do {
            old_top = top.load(std::memory_order_acquire);
            node->next.store(get_unmarked_ref(old_top.ptr), std::memory_order_relaxed);
        } while (!top.compare_exchange_weak(old_top,
                    TaggedPtr{node, old_top.tag + 1},
                    std::memory_order_acq_rel, std::memory_order_acquire));
    }

    int pop() override {
        TaggedPtr old_top;
        while (true) {
            old_top = top.load(std::memory_order_acquire);
            Node* t = get_unmarked_ref(old_top.ptr);
            if (!t) return INT_MIN;
            Node* next = get_unmarked_ref(t->next.load(std::memory_order_acquire));
            if (top.compare_exchange_weak(old_top,
                        TaggedPtr{next, old_top.tag + 1},
                        std::memory_order_acq_rel, std::memory_order_acquire)) {
                int res = t->val;
                delete t;
                return res;
            }
        }
    }

    bool isEmpty() override {
        return get_unmarked_ref(top.load(std::memory_order_acquire).ptr) == nullptr;
    }
};
