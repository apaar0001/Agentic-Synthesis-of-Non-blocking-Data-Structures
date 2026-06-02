#pragma once
#include <atomic>
#include <limits>
#include <random>

class SkipListOptimisticCAS {
private:
    static const int SCALE = 32;

    struct Node {
        const int val;
        const int height;
        std::atomic<uintptr_t>* next;

        Node(int v, int l) : val(v), height(l) {
            next = new std::atomic<uintptr_t>[l + 1];
            for (int i = 0; i <= l; i++) {
                next[i].store(0);
            }
        }

        ~Node() {
            delete[] next;
        }
    };

    static inline uintptr_t combine(Node* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline Node* get_ptr(uintptr_t v) {
        return reinterpret_cast<Node*>(v & ~1);
    }
    static inline bool get_mark(uintptr_t v) {
        return (v & 1) != 0;
    }

    Node* head;
    Node* tail;

    int rnd() {
        static thread_local std::mt19937 r;
        static thread_local std::uniform_int_distribution<int> dist(0, 1);
        int l = 0;
        while (l < SCALE && dist(r) == 0) {
            l++;
        }
        return l;
    }

    bool find(int key, Node** p, Node** s) {
        bool m = false;
    retry:
        while (true) {
            Node* pr = head;
            for (int i = SCALE; i >= 0; i--) {
                uintptr_t cu_val = pr->next[i].load();
                Node* cu = get_ptr(cu_val);
                while (true) {
                    uintptr_t sc_val = cu->next[i].load();
                    Node* sc = get_ptr(sc_val);
                    m = get_mark(sc_val);
                    while (m) {
                        uintptr_t expected = combine(cu, false);
                        if (!pr->next[i].compare_exchange_strong(expected, combine(sc, false))) {
                            goto retry;
                        }
                        cu = sc;
                        sc_val = cu->next[i].load();
                        sc = get_ptr(sc_val);
                        m = get_mark(sc_val);
                    }
                    if (cu->val < key) {
                        pr = cu;
                        cu = sc;
                    } else {
                        break;
                    }
                }
                p[i] = pr;
                s[i] = cu;
            }
            return s[0]->val == key;
        }
    }

public:
    SkipListOptimisticCAS() {
        head = new Node(std::numeric_limits<int>::min(), SCALE);
        tail = new Node(std::numeric_limits<int>::max(), SCALE);
        for (int i = 0; i <= SCALE; i++) {
            head->next[i].store(combine(tail, false));
        }
    }

    bool add(int key) {
        int l = rnd();
        Node* p[SCALE + 1];
        Node* s[SCALE + 1];
        while (true) {
            if (find(key, p, s)) return false;
            Node* newNode = new Node(key, l);
            for (int i = 0; i <= l; i++) {
                newNode->next[i].store(combine(s[i], false));
            }
            uintptr_t expected = combine(s[0], false);
            if (!p[0]->next[0].compare_exchange_strong(expected, combine(newNode, false))) {
                delete newNode;
                continue;
            }
            for (int i = 1; i <= l; i++) {
                while (true) {
                    expected = combine(s[i], false);
                    if (p[i]->next[i].compare_exchange_strong(expected, combine(newNode, false))) {
                        break;
                    }
                    find(key, p, s);
                }
            }
            return true;
        }
    }

    bool remove(int key) {
        Node* p[SCALE + 1];
        Node* s[SCALE + 1];
        while (true) {
            if (!find(key, p, s)) return false;
            Node* n = s[0];
            for (int i = n->height; i >= 1; i--) {
                uintptr_t sc_val = n->next[i].load();
                Node* sc = get_ptr(sc_val);
                bool m = get_mark(sc_val);
                while (!m) {
                    uintptr_t expected = combine(sc, false);
                    n->next[i].compare_exchange_strong(expected, combine(sc, true));
                    sc_val = n->next[i].load();
                    sc = get_ptr(sc_val);
                    m = get_mark(sc_val);
                }
            }
            uintptr_t sc_val = n->next[0].load();
            Node* sc = get_ptr(sc_val);
            bool m = get_mark(sc_val);
            while (true) {
                uintptr_t expected = combine(sc, false);
                if (n->next[0].compare_exchange_strong(expected, combine(sc, true))) {
                    find(key, p, s);
                    return true;
                }
                sc_val = n->next[0].load();
                sc = get_ptr(sc_val);
                m = get_mark(sc_val);
                if (m) return false;
            }
        }
    }

    bool contains(int key) {
        Node* pr = head;
        for (int i = SCALE; i >= 0; i--) {
            uintptr_t cu_val = pr->next[i].load();
            Node* cu = get_ptr(cu_val);
            while (true) {
                uintptr_t sc_val = cu->next[i].load();
                Node* sc = get_ptr(sc_val);
                bool m = get_mark(sc_val);
                while (m) {
                    cu = sc;
                    sc_val = cu->next[i].load();
                    sc = get_ptr(sc_val);
                    m = get_mark(sc_val);
                }
                if (cu->val < key) {
                    pr = cu;
                    cu = sc;
                } else {
                    break;
                }
            }
        }
        uintptr_t cu_val = pr->next[0].load();
        Node* cu = get_ptr(cu_val);
        uintptr_t sc_val = cu->next[0].load();
        return cu->val == key && !get_mark(sc_val);
    }
};
