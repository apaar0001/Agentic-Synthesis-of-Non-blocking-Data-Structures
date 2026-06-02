#pragma once
#include <atomic>
#include <limits>

class LinkedListLazyDeletion {
private:
    struct Node {
        const int key;
        std::atomic<uintptr_t> next; 

        Node(int key) : key(key), next(0) {}
    };

    static inline uintptr_t combine(Node* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline Node* get_ptr(uintptr_t val) {
        return reinterpret_cast<Node*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    Node* head;
    Node* tail;

    struct Window {
        Node* pred;
        Node* curr;
        Window(Node* p, Node* c) : pred(p), curr(c) {}
    };

    Window find(int key) {
        Node *pred, *curr, *succ;
        bool marked;
        while (true) {
        retry:
            pred = head;
            uintptr_t curr_val = pred->next.load();
            curr = get_ptr(curr_val);
            while (true) {
                uintptr_t succ_val = curr->next.load();
                succ = get_ptr(succ_val);
                marked = get_mark(succ_val);
                while (marked) {
                    uintptr_t expected = combine(curr, false);
                    if (!pred->next.compare_exchange_strong(expected, combine(succ, false))) {
                        goto retry;
                    }
                    curr = succ;
                    succ_val = curr->next.load();
                    succ = get_ptr(succ_val);
                    marked = get_mark(succ_val);
                }
                if (curr->key >= key) {
                    return Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

public:
    LinkedListLazyDeletion() {
        head = new Node(std::numeric_limits<int>::min());
        tail = new Node(std::numeric_limits<int>::max());
        head->next.store(combine(tail, false));
    }

    bool add(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr->key == key) return false;
            Node* node = new Node(key);
            node->next.store(combine(w.curr, false));
            uintptr_t expected = combine(w.curr, false);
            if (w.pred->next.compare_exchange_strong(expected, combine(node, false))) {
                return true;
            }
            delete node;
        }
    }

    bool remove(int key) {
        while (true) {
            Window w = find(key);
            if (w.curr->key != key) return false;
            uintptr_t succ_val = w.curr->next.load();
            Node* succ = get_ptr(succ_val);
            uintptr_t expected = combine(succ, false);
            if (w.curr->next.compare_exchange_strong(expected, combine(succ, true))) {
                return true;
            }
        }
    }

    bool contains(int key) {
        Node* curr = head;
        uintptr_t curr_val = curr->next.load();
        while (get_ptr(curr_val)->key < key) {
            curr = get_ptr(curr_val);
            curr_val = curr->next.load();
        }
        curr = get_ptr(curr_val);
        uintptr_t next_val = curr->next.load();
        return curr->key == key && !get_mark(next_val);
    }
};
