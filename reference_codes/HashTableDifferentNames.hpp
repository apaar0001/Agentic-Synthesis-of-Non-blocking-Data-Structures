#pragma once
#include <atomic>
#include <limits>
#include <cmath>

class HashTableDifferentNames {
private:
    struct entry {
        const int val;
        std::atomic<uintptr_t> fwd; 

        entry(int v) : val(v), fwd(0) {}
    };

    static inline uintptr_t combine(entry* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline entry* get_ptr(uintptr_t val) {
        return reinterpret_cast<entry*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    struct slot {
        entry* head;
        entry* tail;

        slot() {
            head = new entry(std::numeric_limits<int>::min());
            tail = new entry(std::numeric_limits<int>::max());
            head->fwd.store(combine(tail, false));
        }

        bool find(int v, entry** p, entry** c) {
            entry *pr, *cu, *su;
            bool marked;
            while (true) {
            retry:
                pr = head;
                uintptr_t cu_val = pr->fwd.load();
                cu = get_ptr(cu_val);
                while (true) {
                    uintptr_t su_val = cu->fwd.load();
                    su = get_ptr(su_val);
                    marked = get_mark(su_val);
                    while (marked) {
                        uintptr_t expected = combine(cu, false);
                        if (!pr->fwd.compare_exchange_strong(expected, combine(su, false))) {
                            goto retry;
                        }
                        cu = su;
                        su_val = cu->fwd.load();
                        su = get_ptr(su_val);
                        marked = get_mark(su_val);
                    }
                    if (cu->val >= v) {
                        *p = pr;
                        *c = cu;
                        return cu->val == v;
                    }
                    pr = cu;
                    cu = su;
                }
            }
        }

        bool insert(int v) {
            entry* p;
            entry* c;
            while (true) {
                if (find(v, &p, &c)) return false;
                entry* e = new entry(v);
                e->fwd.store(combine(c, false));
                uintptr_t expected = combine(c, false);
                if (p->fwd.compare_exchange_strong(expected, combine(e, false))) {
                    return true;
                }
                delete e;
            }
        }

        bool del(int v) {
            entry* p;
            entry* c;
            while (true) {
                if (!find(v, &p, &c)) return false;
                uintptr_t s_val = c->fwd.load();
                entry* s = get_ptr(s_val);
                uintptr_t expected = combine(s, false);
                if (!c->fwd.compare_exchange_strong(expected, combine(s, true))) {
                    continue;
                }
                expected = combine(c, false);
                p->fwd.compare_exchange_strong(expected, combine(s, false));
                return true;
            }
        }

        bool has(int v) {
            entry* cu = head;
            uintptr_t cu_val = cu->fwd.load();
            while (get_ptr(cu_val)->val < v) {
                cu = get_ptr(cu_val);
                cu_val = cu->fwd.load();
            }
            cu = get_ptr(cu_val);
            uintptr_t fwd_val = cu->fwd.load();
            return cu->val == v && !get_mark(fwd_val);
        }
    };

    static const int CAPACITY = 512;
    slot* slots[CAPACITY];

    int hash(int k) {
        return std::abs(k) % CAPACITY;
    }

public:
    HashTableDifferentNames() {
        for (int i = 0; i < CAPACITY; i++) {
            slots[i] = new slot();
        }
    }

    bool add(int key) {
        return slots[hash(key)]->insert(key);
    }

    bool remove(int key) {
        return slots[hash(key)]->del(key);
    }

    bool contains(int key) {
        return slots[hash(key)]->has(key);
    }
};
