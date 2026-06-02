#pragma once
#include <atomic>
#include <limits>
#include <cmath>

class HashTableSentinelNode {
private:
    struct Node {
        const int v;
        std::atomic<uintptr_t> f; 

        Node(int val) : v(val), f(0) {}
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
            h->f.store(combine(t, false));
        }

        struct Pair {
            Node* l;
            Node* r;
            Pair(Node* l, Node* r) : l(l), r(r) {}
        };

        Pair search(int v) {
            Node *l, *r, *s;
            bool m;
            while (true) {
            retry:
                l = h;
                uintptr_t r_val = l->f.load();
                r = get_ptr(r_val);
                while (true) {
                    uintptr_t s_val = r->f.load();
                    s = get_ptr(s_val);
                    m = get_mark(s_val);
                    while (m) {
                        uintptr_t expected = combine(r, false);
                        if (!l->f.compare_exchange_strong(expected, combine(s, false))) {
                            goto retry;
                        }
                        r = s;
                        s_val = r->f.load();
                        s = get_ptr(s_val);
                        m = get_mark(s_val);
                    }
                    if (r->v >= v) {
                        return Pair(l, r);
                    }
                    l = r;
                    r = s;
                }
            }
        }

        bool add(int v) {
            while (true) {
                Pair p = search(v);
                if (p.r->v == v) return false;
                Node* n = new Node(v);
                n->f.store(combine(p.r, false));
                uintptr_t expected = combine(p.r, false);
                if (p.l->f.compare_exchange_strong(expected, combine(n, false))) {
                    return true;
                }
                delete n;
            }
        }

        bool remove(int v) {
            while (true) {
                Pair p = search(v);
                if (p.r->v != v) return false;
                uintptr_t s_val = p.r->f.load();
                Node* s = get_ptr(s_val);
                uintptr_t expected = combine(s, false);
                if (!p.r->f.compare_exchange_strong(expected, combine(s, true))) {
                    continue;
                }
                expected = combine(p.r, false);
                p.l->f.compare_exchange_strong(expected, combine(s, false));
                return true;
            }
        }

        bool contains(int v) {
            Node* c = h;
            uintptr_t c_val = c->f.load();
            while (get_ptr(c_val)->v < v) {
                c = get_ptr(c_val);
                c_val = c->f.load();
            }
            c = get_ptr(c_val);
            uintptr_t f_val = c->f.load();
            return c->v == v && !get_mark(f_val);
        }
    };

    static const int CAPACITY = 512;
    Bucket* buckets[CAPACITY];

    int hash(int k) {
        return std::abs(k) % CAPACITY;
    }

public:
    HashTableSentinelNode() {
        for (int i = 0; i < CAPACITY; i++) {
            buckets[i] = new Bucket();
        }
    }

    bool add(int key) {
        return buckets[hash(key)]->add(key);
    }

    bool remove(int key) {
        return buckets[hash(key)]->remove(key);
    }

    bool contains(int key) {
        return buckets[hash(key)]->contains(key);
    }
};
