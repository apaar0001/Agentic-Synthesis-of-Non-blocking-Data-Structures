#pragma once
#include <atomic>
#include <limits>
#include <cmath>

class HashTableOptimisticCAS {
private:
    struct Node {
        const int k;
        std::atomic<uintptr_t> n; 

        Node(int key) : k(key), n(0) {}
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

    struct Bucket {
        Node* h;
        Node* t;

        Bucket() {
            h = new Node(std::numeric_limits<int>::min());
            t = new Node(std::numeric_limits<int>::max());
            h->n.store(combine(t, false));
        }

        void locate(int k, Node** w) {
            Node *p, *c, *s;
            bool m;
            while (true) {
            retry:
                p = h;
                uintptr_t c_val = p->n.load();
                c = get_ptr(c_val);
                while (true) {
                    uintptr_t s_val = c->n.load();
                    s = get_ptr(s_val);
                    m = get_mark(s_val);
                    while (m) {
                        uintptr_t expected = combine(c, false);
                        if (!p->n.compare_exchange_strong(expected, combine(s, false))) {
                            goto retry;
                        }
                        c = s;
                        s_val = c->n.load();
                        s = get_ptr(s_val);
                        m = get_mark(s_val);
                    }
                    if (c->k >= k) {
                        w[0] = p;
                        w[1] = c;
                        return;
                    }
                    p = c;
                    c = s;
                }
            }
        }

        bool add(int k) {
            Node* w[2];
            while (true) {
                locate(k, w);
                if (w[1]->k == k) return false;
                Node* nn = new Node(k);
                nn->n.store(combine(w[1], false));
                uintptr_t expected = combine(w[1], false);
                if (w[0]->n.compare_exchange_strong(expected, combine(nn, false))) {
                    return true;
                }
                delete nn;
            }
        }

        bool remove(int k) {
            Node* w[2];
            while (true) {
                locate(k, w);
                if (w[1]->k != k) return false;
                uintptr_t s_val = w[1]->n.load();
                Node* s = get_ptr(s_val);
                uintptr_t expected = combine(s, false);
                if (!w[1]->n.compare_exchange_strong(expected, combine(s, true))) {
                    continue;
                }
                expected = combine(w[1], false);
                w[0]->n.compare_exchange_strong(expected, combine(s, false));
                return true;
            }
        }

        bool contains(int k) {
            Node* c = h;
            uintptr_t c_val = c->n.load();
            while (get_ptr(c_val)->k < k) {
                c = get_ptr(c_val);
                c_val = c->n.load();
            }
            c = get_ptr(c_val);
            uintptr_t next_val = c->n.load();
            return c->k == k && !get_mark(next_val);
        }
    };

    static const int CAPACITY = 512;
    Bucket* b[CAPACITY];

    int hash(int k) {
        return std::abs(k) % CAPACITY;
    }

public:
    HashTableOptimisticCAS() {
        for (int i = 0; i < CAPACITY; i++) {
            b[i] = new Bucket();
        }
    }

    bool add(int key) {
        return b[hash(key)]->add(key);
    }

    bool remove(int key) {
        return b[hash(key)]->remove(key);
    }

    bool contains(int key) {
        return b[hash(key)]->contains(key);
    }
};
