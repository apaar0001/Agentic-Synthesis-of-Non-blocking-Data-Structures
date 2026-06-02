package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Hash Table — Different Names chaining (Ref 5).
 *
 * Per-bucket uses: entry class / val field / fwd pointer / slot class
 * Bucket ops: insert() / delete() / has() (not add/remove/contains)
 * Traversal: find() returns results via entry[] arrays (out-param style)
 */
public class HashTableDifferentNames implements SetADT {

    private static class entry {
        final int val;
        final AtomicMarkableReference<entry> fwd;

        entry(int v) {
            val = v;
            fwd = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class slot {
        final entry head = new entry(Integer.MIN_VALUE);
        final entry tail = new entry(Integer.MAX_VALUE);

        slot() {
            head.fwd.set(tail, false);
        }

        private boolean find(int v, entry[] p, entry[] c) {
            entry pr, cu, su;
            boolean[] m = { false };
            retry: while (true) {
                pr = head;
                cu = pr.fwd.getReference();
                while (true) {
                    su = cu.fwd.get(m);
                    while (m[0]) {
                        if (!pr.fwd.compareAndSet(cu, su, false, false))
                            continue retry;
                        cu = su;
                        su = cu.fwd.get(m);
                    }
                    if (cu.val >= v) {
                        p[0] = pr;
                        c[0] = cu;
                        return cu.val == v;
                    }
                    pr = cu;
                    cu = su;
                }
            }
        }

        boolean insert(int v) {
            entry[] p = new entry[1], c = new entry[1];
            while (true) {
                if (find(v, p, c))
                    return false;
                entry e = new entry(v);
                e.fwd.set(c[0], false);
                if (p[0].fwd.compareAndSet(c[0], e, false, false))
                    return true;
            }
        }

        boolean delete(int v) {
            entry[] p = new entry[1], c = new entry[1];
            while (true) {
                if (!find(v, p, c))
                    return false;
                entry s = c[0].fwd.getReference();
                if (!c[0].fwd.compareAndSet(s, s, false, true))
                    continue;
                // Node has been marked
                p[0].fwd.compareAndSet(c[0], s, false, false);
                return true;
            }
        }

        boolean has(int v) {
            boolean[] m = { false };
            entry cu = head;
            while (cu.val < v)
                cu = cu.fwd.getReference();
            cu.fwd.get(m);
            return cu.val == v && !m[0];
        }
    }

    private static final int CAPACITY = 512;
    private final slot[] slots = new slot[CAPACITY];

    public HashTableDifferentNames() {
        for (int i = 0; i < CAPACITY; i++)
            slots[i] = new slot();
    }

    private int hash(int k) {
        return Math.abs(k) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        return slots[hash(key)].insert(key);
    }

    @Override
    public boolean remove(int key) {
        return slots[hash(key)].delete(key);
    }

    @Override
    public boolean contains(int key) {
        return slots[hash(key)].has(key);
    }
}
