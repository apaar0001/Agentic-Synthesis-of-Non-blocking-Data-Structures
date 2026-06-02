#pragma once
#include <atomic>
#include <limits>
#include <random>

class SkipListSentinelNode {
private:
    static const int MAX_LVL = 31;

    struct Node {
        const int val;
        const int height;
        std::atomic<uintptr_t>* forward;

        Node(int v, int l) : val(v), height(l) {
            forward = new std::atomic<uintptr_t>[l + 1];
            for (int i = 0; i <= l; i++) {
                forward[i].store(0);
            }
        }

        ~Node() {
            delete[] forward;
        }
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

    int rndLvl() {
        static thread_local std::mt19937 random;
        static thread_local std::uniform_int_distribution<int> dist(0, 1);
        int l = 0;
        while (l < MAX_LVL && dist(random) == 0) {
            l++;
        }
        return l;
    }

    bool search(int key, Node** p, Node** s) {
        bool m = false;
    retry:
        while (true) {
            Node* currP = head;
            for (int i = MAX_LVL; i >= 0; i--) {
                uintptr_t curr_val = currP->forward[i].load();
                Node* curr = get_ptr(curr_val);
                while (true) {
                    uintptr_t next_val = curr->forward[i].load();
                    Node* next = get_ptr(next_val);
                    m = get_mark(next_val);
                    while (m) {
                        uintptr_t expected = combine(curr, false);
                        if (!currP->forward[i].compare_exchange_strong(expected, combine(next, false))) {
                            goto retry;
                        }
                        curr = next;
                        next_val = curr->forward[i].load();
                        next = get_ptr(next_val);
                        m = get_mark(next_val);
                    }
                    if (curr->val < key) {
                        currP = curr;
                        curr = next;
                    } else {
                        break;
                    }
                }
                p[i] = currP;
                s[i] = curr;
            }
            return s[0]->val == key;
        }
    }

public:
    SkipListSentinelNode() {
        head = new Node(std::numeric_limits<int>::min(), MAX_LVL);
        tail = new Node(std::numeric_limits<int>::max(), MAX_LVL);
        for (int i = 0; i <= MAX_LVL; i++) {
            head->forward[i].store(combine(tail, false));
        }
    }

    bool add(int key) {
        int l = rndLvl();
        Node* p[MAX_LVL + 1];
        Node* s[MAX_LVL + 1];
        while (true) {
            if (search(key, p, s)) return false;
            Node* newNode = new Node(key, l);
            for (int i = 0; i <= l; i++) {
                newNode->forward[i].store(combine(s[i], false));
            }
            uintptr_t expected = combine(s[0], false);
            if (!p[0]->forward[0].compare_exchange_strong(expected, combine(newNode, false))) {
                delete newNode;
                continue;
            }
            for (int i = 1; i <= l; i++) {
                while (true) {
                    expected = combine(s[i], false);
                    if (p[i]->forward[i].compare_exchange_strong(expected, combine(newNode, false))) {
                        break;
                    }
                    search(key, p, s);
                }
            }
            return true;
        }
    }

    bool remove(int key) {
        Node* p[MAX_LVL + 1];
        Node* s[MAX_LVL + 1];
        while (true) {
            if (!search(key, p, s)) return false;
            Node* node = s[0];
            for (int i = node->height; i >= 1; i--) {
                uintptr_t next_val = node->forward[i].load();
                Node* next = get_ptr(next_val);
                bool m = get_mark(next_val);
                while (!m) {
                    uintptr_t expected = combine(next, false);
                    node->forward[i].compare_exchange_strong(expected, combine(next, true));
                    next_val = node->forward[i].load();
                    next = get_ptr(next_val);
                    m = get_mark(next_val);
                }
            }
            uintptr_t next_val = node->forward[0].load();
            Node* next = get_ptr(next_val);
            bool m = get_mark(next_val);
            while (true) {
                uintptr_t expected = combine(next, false);
                if (node->forward[0].compare_exchange_strong(expected, combine(next, true))) {
                    search(key, p, s);
                    return true;
                }
                next_val = node->forward[0].load();
                next = get_ptr(next_val);
                m = get_mark(next_val);
                if (m) return false;
            }
        }
    }

    bool contains(int key) {
        Node* cP = head;
        for (int i = MAX_LVL; i >= 0; i--) {
            uintptr_t c_val = cP->forward[i].load();
            Node* c = get_ptr(c_val);
            while (true) {
                uintptr_t s_val = c->forward[i].load();
                Node* s = get_ptr(s_val);
                bool m = get_mark(s_val);
                while (m) {
                    c = s;
                    s_val = c->forward[i].load();
                    s = get_ptr(s_val);
                    m = get_mark(s_val);
                }
                if (c->val < key) {
                    cP = c;
                    c = s;
                } else {
                    break;
                }
            }
        }
        uintptr_t c_val = cP->forward[0].load();
        Node* c = get_ptr(c_val);
        uintptr_t s_val = c->forward[0].load();
        return c->val == key && !get_mark(s_val);
    }
};
