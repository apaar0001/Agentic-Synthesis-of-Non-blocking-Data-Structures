package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Hash Table — Optimistic CAS chaining (Ref 4).
 *
 * Per-bucket: sorted lock-free list using AtomicMarkableReference.
 * Names: Node/k/n (single-char fields), Bucket/b[], capacity=512
 *
 * Original ref4 used plain AtomicReference (not lock-free: concurrent removes
 * can leave dangling pointers). Rewritten with AtomicMarkableReference to be
 * truly lock-free while preserving the short field name style.
 */
public class HashTableOptimisticCAS implements SetADT {

    private static class Node {
        final int k;
        final AtomicMarkableReference<Node> n;

        Node(int key) {
            k = key;
            n = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        final Node h = new Node(Integer.MIN_VALUE);
        final Node t = new Node(Integer.MAX_VALUE);

        Bucket() {
            h.n.set(t, false);
        }

        // Returns (pred, curr) window
        private Node[] locate(int k) {
            Node[] w = new Node[2];
            boolean[] m = { false };
            retry: while (true) {
                Node p = h;
                Node c = h.n.getReference();
                while (true) {
                    Node s = c.n.get(m);
                    while (m[0]) {
                        if (!p.n.compareAndSet(c, s, false, false))
                            continue retry;
                        c = s;
                        s = c.n.get(m);
                    }
                    if (c.k >= k) {
                        w[0] = p;
                        w[1] = c;
                        return w;
                    }
                    p = c;
                    c = s;
                }
            }
        }

        boolean add(int k) {
            while (true) {
                Node[] w = locate(k);
                if (w[1].k == k)
                    return false;
                Node nn = new Node(k);
                nn.n.set(w[1], false);
                if (w[0].n.compareAndSet(w[1], nn, false, false))
                    return true;
            }
        }

        boolean remove(int k) {
            while (true) {
                Node[] w = locate(k);
                if (w[1].k != k)
                    return false;
                Node s = w[1].n.getReference();
                if (!w[1].n.compareAndSet(s, s, false, true))
                    continue;
                // Node has been marked
                w[0].n.compareAndSet(w[1], s, false, false);
                return true;
            }
        }

        boolean contains(int k) {
            boolean[] m = { false };
            Node c = h;
            while (c.k < k)
                c = c.n.getReference();
            c.n.get(m);
            return c.k == k && !m[0];
        }
    }

    private static final int CAPACITY = 512;
    private final Bucket[] b = new Bucket[CAPACITY];

    public HashTableOptimisticCAS() {
        for (int i = 0; i < CAPACITY; i++)
            b[i] = new Bucket();
    }

    private int hash(int k) {
        return Math.abs(k) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        return b[hash(key)].add(key);
    }

    @Override
    public boolean remove(int key) {
        return b[hash(key)].remove(key);
    }

    @Override
    public boolean contains(int key) {
        return b[hash(key)].contains(key);
    }
}
