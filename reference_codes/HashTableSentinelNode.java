package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Hash Table — Sentinel Node chaining (Ref 3).
 *
 * Per-bucket: sorted lock-free list with heavily abbreviated names.
 * Names: Node/v/f (val/forward), Bucket/Pair-l-r/search(), capacity=512
 *
 * Distinct from ref2: Pair (l/r) instead of Window (p/c), search() not find(),
 * single-char field names v/f.
 */
public class HashTableSentinelNode implements SetADT {

    private static class Node {
        final int v;
        final AtomicMarkableReference<Node> f;

        Node(int val) {
            v = val;
            f = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        final Node h = new Node(Integer.MIN_VALUE);
        final Node t = new Node(Integer.MAX_VALUE);

        Bucket() {
            h.f.set(t, false);
        }

        private static class Pair {
            Node l, r;

            Pair(Node l, Node r) {
                this.l = l;
                this.r = r;
            }
        }

        private Pair search(int v) {
            Node l, r, s;
            boolean[] m = { false };
            retry: while (true) {
                l = h;
                r = l.f.getReference();
                while (true) {
                    s = r.f.get(m);
                    while (m[0]) {
                        if (!l.f.compareAndSet(r, s, false, false))
                            continue retry;
                        r = s;
                        s = r.f.get(m);
                    }
                    if (r.v >= v)
                        return new Pair(l, r);
                    l = r;
                    r = s;
                }
            }
        }

        boolean add(int v) {
            while (true) {
                Pair p = search(v);
                if (p.r.v == v)
                    return false;
                Node n = new Node(v);
                n.f.set(p.r, false);
                if (p.l.f.compareAndSet(p.r, n, false, false))
                    return true;
            }
        }

        boolean remove(int v) {
            while (true) {
                Pair p = search(v);
                if (p.r.v != v)
                    return false;
                Node s = p.r.f.getReference();
                if (!p.r.f.compareAndSet(s, s, false, true))
                    continue;
                // Node has been marked
                p.l.f.compareAndSet(p.r, s, false, false);
                return true;
            }
        }

        boolean contains(int v) {
            boolean[] m = { false };
            Node c = h;
            while (c.v < v)
                c = c.f.getReference();
            c.f.get(m);
            return c.v == v && !m[0];
        }
    }

    private static final int CAPACITY = 512;
    private final Bucket[] buckets = new Bucket[CAPACITY];

    public HashTableSentinelNode() {
        for (int i = 0; i < CAPACITY; i++)
            buckets[i] = new Bucket();
    }

    private int hash(int k) {
        return Math.abs(k) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        return buckets[hash(key)].add(key);
    }

    @Override
    public boolean remove(int key) {
        return buckets[hash(key)].remove(key);
    }

    @Override
    public boolean contains(int key) {
        return buckets[hash(key)].contains(key);
    }
}
