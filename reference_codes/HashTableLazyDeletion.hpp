#pragma once
#include <atomic>
#include <limits>
#include <cmath>

class HashTableLazyDeletion {
private:
    struct Node {
        const int key;
        std::atomic<uintptr_t> next; 

        Node(int k) : key(k), next(0) {}
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
            h->next.store(combine(t, false));
        }

        struct Window {
            Node* p;
            Node* c;
            Window(Node* p, Node* c) : p(p), c(c) {}
        };

        Window find(int k) {
            Node *p, *c, *s;
            bool m;
            while (true) {
            retry:
                p = h;
                uintptr_t c_val = p->next.load();
                c = get_ptr(c_val);
                while (true) {
                    uintptr_t s_val = c->next.load();
                    s = get_ptr(s_val);
                    m = get_mark(s_val);
                    while (m) {
                        uintptr_t expected = combine(c, false);
                        if (!p->next.compare_exchange_strong(expected, combine(s, false))) {
                            goto retry;
                        }
                        c = s;
                        s_val = c->next.load();
                        s = get_ptr(s_val);
                        m = get_mark(s_val);
                    }
                    if (c->key >= k) {
                        return Window(p, c);
                    }
                    p = c;
                    c = s;
                }
            }
        }

        bool add(int k) {
            while (true) {
                Window w = find(k);
                if (w.c->key == k) return false;
                Node* n = new Node(k);
                n->next.store(combine(w.c, false));
                uintptr_t expected = combine(w.c, false);
                if (w.p->next.compare_exchange_strong(expected, combine(n, false))) {
                    return true;
                }
                delete n;
            }
        }

        bool remove(int k) {
            while (true) {
                Window w = find(k);
                if (w.c->key != k) return false;
                uintptr_t s_val = w.c->next.load();
                Node* s = get_ptr(s_val);
                uintptr_t expected = combine(s, false);
                if (w.c->next.compare_exchange_strong(expected, combine(s, true))) {
                    return true;
                }
            }
        }

        bool contains(int k) {
            Node* c = h;
            uintptr_t c_val = c->next.load();
            while (get_ptr(c_val)->key < k) {
                c = get_ptr(c_val);
                c_val = c->next.load();
            }
            c = get_ptr(c_val);
            uintptr_t next_val = c->next.load();
            return c->key == k && !get_mark(next_val);
        }
    };

    static const int CAPACITY = 512;
    Bucket* table[CAPACITY];

    int hash(int k) {
        return std::abs(k) % CAPACITY;
    }

public:
    HashTableLazyDeletion() {
        for (int i = 0; i < CAPACITY; i++) {
            table[i] = new Bucket();
        }
    }

    bool add(int key) {
        return table[hash(key)]->add(key);
    }

    bool remove(int key) {
        return table[hash(key)]->remove(key);
    }

    bool contains(int key) {
        return table[hash(key)]->contains(key);
    }
};
