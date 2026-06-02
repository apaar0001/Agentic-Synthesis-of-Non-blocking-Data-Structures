#pragma once
#include <atomic>
#include <limits>

class LinkedListDifferentNames {
private:
    struct Element {
        const int data;
        std::atomic<uintptr_t> nextElement; 

        Element(int d) : data(d), nextElement(0) {}
    };

    static inline uintptr_t combine(Element* ptr, bool mark) {
        return (reinterpret_cast<uintptr_t>(ptr) & ~1) | (mark ? 1 : 0);
    }
    static inline Element* get_ptr(uintptr_t val) {
        return reinterpret_cast<Element*>(val & ~1);
    }
    static inline bool get_mark(uintptr_t val) {
        return (val & 1) != 0;
    }

    Element* anchor;
    Element* boundary;

    struct ScanResult {
        Element* p;
        Element* c;
        ScanResult(Element* p, Element* c) : p(p), c(c) {}
    };

    ScanResult scan(int d) {
        Element *p, *c, *s;
        bool m;
        while (true) {
        retry:
            p = anchor;
            uintptr_t c_val = p->nextElement.load();
            c = get_ptr(c_val);
            while (true) {
                uintptr_t s_val = c->nextElement.load();
                s = get_ptr(s_val);
                m = get_mark(s_val);
                while (m) {
                    uintptr_t expected = combine(c, false);
                    if (!p->nextElement.compare_exchange_strong(expected, combine(s, false))) {
                        goto retry;
                    }
                    c = s;
                    s_val = c->nextElement.load();
                    s = get_ptr(s_val);
                    m = get_mark(s_val);
                }
                if (c->data >= d) {
                    return ScanResult(p, c);
                }
                p = c;
                c = s;
            }
        }
    }

public:
    LinkedListDifferentNames() {
        anchor = new Element(std::numeric_limits<int>::min());
        boundary = new Element(std::numeric_limits<int>::max());
        anchor->nextElement.store(combine(boundary, false));
    }

    bool add(int key) {
        while (true) {
            ScanResult res = scan(key);
            if (res.c->data == key) return false;
            Element* e = new Element(key);
            e->nextElement.store(combine(res.c, false));
            uintptr_t expected = combine(res.c, false);
            if (res.p->nextElement.compare_exchange_strong(expected, combine(e, false))) {
                return true;
            }
            delete e;
        }
    }

    bool remove(int key) {
        while (true) {
            ScanResult res = scan(key);
            if (res.c->data != key) return false;
            uintptr_t s_val = res.c->nextElement.load();
            Element* s = get_ptr(s_val);
            uintptr_t expected = combine(s, false);
            if (!res.c->nextElement.compare_exchange_strong(expected, combine(s, true))) {
                continue;
            }
            expected = combine(res.c, false);
            res.p->nextElement.compare_exchange_strong(expected, combine(s, false));
            return true;
        }
    }

    bool contains(int key) {
        Element* c = anchor;
        uintptr_t c_val = c->nextElement.load();
        while (get_ptr(c_val)->data < key) {
            c = get_ptr(c_val);
            c_val = c->nextElement.load();
        }
        c = get_ptr(c_val);
        uintptr_t m_val = c->nextElement.load();
        return c->data == key && !get_mark(m_val);
    }
};
