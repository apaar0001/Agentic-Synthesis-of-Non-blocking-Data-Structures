#pragma once
#include <atomic>
#include <limits>

class LinkedListSentinelNode {
private:
    struct ListNode {
        const int value;
        std::atomic<uintptr_t> next; 

        ListNode(int val) : value(val), next(0) {}
    };

    static inline uintptr_t combine(ListNode* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline ListNode* get_ptr(uintptr_t val) {
        return reinterpret_cast<ListNode*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    ListNode* head;
    ListNode* tail;

    struct NodePair {
        ListNode* left;
        ListNode* right;
        NodePair(ListNode* l, ListNode* r) : left(l), right(r) {}
    };

    NodePair search(int val) {
        ListNode *left, *right, *succ;
        bool isMarked;
        while (true) {
        retry:
            left = head;
            uintptr_t right_val = left->next.load();
            right = get_ptr(right_val);
            while (true) {
                uintptr_t succ_val = right->next.load();
                succ = get_ptr(succ_val);
                isMarked = get_mark(succ_val);
                while (isMarked) {
                    uintptr_t expected = combine(right, false);
                    if (!left->next.compare_exchange_strong(expected, combine(succ, false))) {
                        goto retry;
                    }
                    right = succ;
                    succ_val = right->next.load();
                    succ = get_ptr(succ_val);
                    isMarked = get_mark(succ_val);
                }
                if (right->value >= val) {
                    return NodePair(left, right);
                }
                left = right;
                right = succ;
            }
        }
    }

public:
    LinkedListSentinelNode() {
        head = new ListNode(std::numeric_limits<int>::min());
        tail = new ListNode(std::numeric_limits<int>::max());
        head->next.store(combine(tail, false));
    }

    bool add(int key) {
        while (true) {
            NodePair pair = search(key);
            if (pair.right->value == key) return false;
            ListNode* newNode = new ListNode(key);
            newNode->next.store(combine(pair.right, false));
            uintptr_t expected = combine(pair.right, false);
            if (pair.left->next.compare_exchange_strong(expected, combine(newNode, false))) {
                return true;
            }
            delete newNode;
        }
    }

    bool remove(int key) {
        while (true) {
            NodePair pair = search(key);
            if (pair.right->value != key) return false;
            uintptr_t succ_val = pair.right->next.load();
            ListNode* succ = get_ptr(succ_val);
            uintptr_t expected = combine(succ, false);
            if (!pair.right->next.compare_exchange_strong(expected, combine(succ, true))) {
                continue;
            }
            expected = combine(pair.right, false);
            pair.left->next.compare_exchange_strong(expected, combine(succ, false));
            return true;
        }
    }

    bool contains(int key) {
        ListNode* curr = head;
        uintptr_t curr_val = curr->next.load();
        while (get_ptr(curr_val)->value < key) {
            curr = get_ptr(curr_val);
            curr_val = curr->next.load();
        }
        curr = get_ptr(curr_val);
        uintptr_t next_val = curr->next.load();
        return curr->value == key && !get_mark(next_val);
    }
};
