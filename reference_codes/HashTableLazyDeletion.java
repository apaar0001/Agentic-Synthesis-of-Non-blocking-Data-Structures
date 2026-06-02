package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

/**
 * Lock-free Hash Table — Lazy Deletion chaining (Ref 2).
 *
 * Per-bucket: sorted lock-free list where remove() only marks (lazy),
 * no physical removal in remove() itself. Cleanup deferred to find().
 * Names: Node/key-k/next, Bucket/Window-p-c/find(), capacity=512
 */
public class HashTableLazyDeletion implements SetADT {

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int k) {
            key = k;
            next = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        final Node h = new Node(Integer.MIN_VALUE);
        final Node t = new Node(Integer.MAX_VALUE);

        Bucket() {
            h.next.set(t, false);
        }

        private static class Window {
            Node p, c;

            Window(Node p, Node c) {
                this.p = p;
                this.c = c;
            }
        }

        private Window find(int k) {
            Node p, c, s;
            boolean[] m = { false };
            retry: while (true) {
                p = h;
                c = p.next.getReference();
                while (true) {
                    s = c.next.get(m);
                    while (m[0]) {
                        if (!p.next.compareAndSet(c, s, false, false))
                            continue retry;
                        c = s;
                        s = c.next.get(m);
                    }
                    if (c.key >= k)
                        return new Window(p, c);
                    p = c;
                    c = s;
                }
            }
        }

        boolean add(int k) {
            while (true) {
                Window w = find(k);
                if (w.c.key == k)
                    return false;
                Node n = new Node(k);
                n.next.set(w.c, false);
                if (w.p.next.compareAndSet(w.c, n, false, false))
                    return true;
            }
        }

        boolean remove(int k) {
            while (true) {
                Window w = find(k);
                if (w.c.key != k)
                    return false;
                Node s = w.c.next.getReference();
                // Lazy: mark only, no physical removal here
                if (w.c.next.compareAndSet(s, s, false, true)) {
                    // Node has been marked
                    return true;
                }
            }
        }

        boolean contains(int k) {
            boolean[] m = { false };
            Node c = h;
            while (c.key < k)
                c = c.next.getReference();
            c.next.get(m);
            return c.key == k && !m[0];
        }
    }

    private static final int CAPACITY = 512;
    private final Bucket[] table = new Bucket[CAPACITY];

    public HashTableLazyDeletion() {
        for (int i = 0; i < CAPACITY; i++)
            table[i] = new Bucket();
    }

    private int hash(int k) {
        return Math.abs(k) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        return table[hash(key)].add(key);
    }

    @Override
    public boolean remove(int key) {
        return table[hash(key)].remove(key);
    }

    @Override
    public boolean contains(int key) {
        return table[hash(key)].contains(key);
    }
}
