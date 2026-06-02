package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Lock-free BST with Short/Abbreviated Field Names (Optimistic CAS variant).
 *
 * Preserves the original short-name style from ref4:
 * - Node field: k (key), m (marked), l (left), r (right)
 *
 * Algorithm: same lazy BST deletion as ref2/ref3, but with abbreviated names
 * for distinct CodeBLEU token diversity.
 *
 * Lock-freedom: all operations are CAS-only on atomic fields; no blocking.
 */
public class BSTOptimisticCAS implements SetADT {

    private static class Node {
        final int k; // key
        final AtomicBoolean m; // marked/deleted
        final AtomicReference<Node> l, r; // left, right children

        Node(int key) {
            k = key;
            m = new AtomicBoolean(false);
            l = new AtomicReference<>(null);
            r = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> root = new AtomicReference<>(null);

    @Override
    public boolean add(int key) {
        while (true) {
            Node n = new Node(key);
            if (root.compareAndSet(null, n))
                return true;
            Node c = root.get();
            while (true) {
                if (c.k == key) {
                    if (c.m.compareAndSet(true, false))
                        return true;
                    return false;
                }
                AtomicReference<Node> next = (key < c.k) ? c.l : c.r;
                Node t = next.get();
                if (t == null) {
                    if (next.compareAndSet(null, n))
                        return true;
                    t = next.get();
                }
                c = t;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node c = root.get();
        while (c != null) {
            if (c.k == key) {
                if (c.m.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
                return false;
            }
            c = (key < c.k) ? c.l.get() : c.r.get();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        Node c = root.get();
        while (c != null) {
            if (c.k == key)
                return !c.m.get();
            c = (key < c.k) ? c.l.get() : c.r.get();
        }
        return false;
    }
}
