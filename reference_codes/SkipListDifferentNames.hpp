#pragma once
#include <atomic>
#include <limits>
#include <random>

class SkipListDifferentNames {
private:
    static const int MAX_H = 31;

    struct Tower {
        const int item;
        const int height;
        std::atomic<uintptr_t>* levels;

        Tower(int i, int h) : item(i), height(h) {
            levels = new std::atomic<uintptr_t>[h + 1];
            for (int k = 0; k <= h; k++) {
                levels[k].store(0);
            }
        }
        
        ~Tower() {
            delete[] levels;
        }
    };

    static inline uintptr_t combine(Tower* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline Tower* get_ptr(uintptr_t val) {
        return reinterpret_cast<Tower*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    Tower* headTower;
    Tower* tailTower;

    int rnd() {
        static thread_local std::mt19937 generator;
        static thread_local std::uniform_int_distribution<int> distribution(0, 1);
        int l = 0;
        while (l < MAX_H && distribution(generator) == 0) {
            l++;
        }
        return l;
    }

    bool locate(int key, Tower** p, Tower** s) {
        bool m = false;
    retry:
        while (true) {
            Tower* pt = headTower;
            for (int i = MAX_H; i >= 0; i--) {
                uintptr_t ct_val = pt->levels[i].load();
                Tower* ct = get_ptr(ct_val);
                while (true) {
                    uintptr_t nt_val = ct->levels[i].load();
                    Tower* nt = get_ptr(nt_val);
                    m = get_mark(nt_val);
                    while (m) {
                        uintptr_t expected = combine(ct, false);
                        if (!pt->levels[i].compare_exchange_strong(expected, combine(nt, false))) {
                            goto retry;
                        }
                        ct = nt;
                        nt_val = ct->levels[i].load();
                        nt = get_ptr(nt_val);
                        m = get_mark(nt_val);
                    }
                    if (ct->item < key) {
                        pt = ct;
                        ct = nt;
                    } else {
                        break;
                    }
                }
                p[i] = pt;
                s[i] = ct;
            }
            return s[0]->item == key;
        }
    }

public:
    SkipListDifferentNames() {
        headTower = new Tower(std::numeric_limits<int>::min(), MAX_H);
        tailTower = new Tower(std::numeric_limits<int>::max(), MAX_H);
        for (int i = 0; i <= MAX_H; i++) {
            headTower->levels[i].store(combine(tailTower, false));
        }
    }

    bool add(int key) {
        int h = rnd();
        Tower* p[MAX_H + 1];
        Tower* s[MAX_H + 1];
        while (true) {
            if (locate(key, p, s)) return false;
            Tower* newT = new Tower(key, h);
            for (int i = 0; i <= h; i++) {
                newT->levels[i].store(combine(s[i], false));
            }
            uintptr_t expected = combine(s[0], false);
            if (!p[0]->levels[0].compare_exchange_strong(expected, combine(newT, false))) {
                delete newT;
                continue;
            }
            for (int i = 1; i <= h; i++) {
                while (true) {
                    expected = combine(s[i], false);
                    if (p[i]->levels[i].compare_exchange_strong(expected, combine(newT, false))) {
                        break;
                    }
                    locate(key, p, s);
                }
            }
            return true;
        }
    }

    bool remove(int key) {
        Tower* p[MAX_H + 1];
        Tower* s[MAX_H + 1];
        while (true) {
            if (!locate(key, p, s)) return false;
            Tower* t = s[0];
            for (int i = t->height; i >= 1; i--) {
                uintptr_t nt_val = t->levels[i].load();
                Tower* nt = get_ptr(nt_val);
                bool m = get_mark(nt_val);
                while (!m) {
                    uintptr_t expected = combine(nt, false);
                    t->levels[i].compare_exchange_strong(expected, combine(nt, true));
                    nt_val = t->levels[i].load();
                    nt = get_ptr(nt_val);
                    m = get_mark(nt_val);
                }
            }
            uintptr_t nt_val = t->levels[0].load();
            Tower* nt = get_ptr(nt_val);
            bool m = get_mark(nt_val);
            while (true) {
                uintptr_t expected = combine(nt, false);
                if (t->levels[0].compare_exchange_strong(expected, combine(nt, true))) {
                    locate(key, p, s);
                    return true;
                }
                nt_val = t->levels[0].load();
                nt = get_ptr(nt_val);
                m = get_mark(nt_val);
                if (m) return false;
            }
        }
    }

    bool contains(int key) {
        Tower* pt = headTower;
        for (int i = MAX_H; i >= 0; i--) {
            uintptr_t ct_val = pt->levels[i].load();
            Tower* ct = get_ptr(ct_val);
            while (true) {
                uintptr_t nt_val = ct->levels[i].load();
                Tower* nt = get_ptr(nt_val);
                bool m = get_mark(nt_val);
                while (m) {
                    ct = nt;
                    nt_val = ct->levels[i].load();
                    nt = get_ptr(nt_val);
                    m = get_mark(nt_val);
                }
                if (ct->item < key) {
                    pt = ct;
                    ct = nt;
                } else {
                    break;
                }
            }
        }
        uintptr_t ct_val = pt->levels[0].load();
        Tower* ct = get_ptr(ct_val);
        uintptr_t nt_val = ct->levels[0].load();
        return ct->item == key && !get_mark(nt_val);
    }
};
