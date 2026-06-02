package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

/**
 * Lock-free Skip List — Optimistic CAS variant.
 * Names: Node / next[] / val / SCALE=32 / find() with short p/s/cu/sc vars
 *
 * Distinct from ref1 (HarrisMichael) in:
 * - Constant name: SCALE instead of MAX_LEVEL
 * - Inner loop variable names: pr/cu/sc instead of pred/curr/succ
 * - remove() loop reads from n.next directly (not via succs[0].next)
 */
public class SkipListOptimisticCAS implements SetADT {

    private static final int SCALE = 32;

    private static class Node {
        final int val;
        @SuppressWarnings("unchecked")
        final AtomicMarkableReference<Node>[] next;

        @SuppressWarnings("unchecked")
        Node(int v, int l) {
            val = v;
            next = new AtomicMarkableReference[l + 1];
            for (int i = 0; i <= l; i++)
                next[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;
    private final Random r = new Random();

    public SkipListOptimisticCAS() {
        head = new Node(Integer.MIN_VALUE, SCALE);
        tail = new Node(Integer.MAX_VALUE, SCALE);
        for (int i = 0; i <= SCALE; i++)
            head.next[i].set(tail, false);
    }

    private boolean find(int key, Node[] p, Node[] s) {
        boolean[] m = { false };
        retry: while (true) {
            Node pr = head;
            for (int i = SCALE; i >= 0; i--) {
                Node cu = pr.next[i].getReference();
                while (true) {
                    Node sc = cu.next[i].get(m);
                    while (m[0]) {
                        if (!pr.next[i].compareAndSet(cu, sc, false, false))
                            continue retry;
                        cu = sc;
                        sc = cu.next[i].get(m);
                    }
                    if (cu.val < key) {
                        pr = cu;
                        cu = sc;
                    } else
                        break;
                }
                p[i] = pr;
                s[i] = cu;
            }
            return s[0].val == key;
        }
    }

    @Override
    public boolean add(int key) {
        int l = rnd();
        Node[] p = new Node[SCALE + 1], s = new Node[SCALE + 1];
        while (true) {
            if (find(key, p, s))
                return false;
            Node newNode = new Node(key, l);
            for (int i = 0; i <= l; i++)
                newNode.next[i].set(s[i], false);
            if (!p[0].next[0].compareAndSet(s[0], newNode, false, false))
                continue;
            for (int i = 1; i <= l; i++) {
                while (!p[i].next[i].compareAndSet(s[i], newNode, false, false))
                    find(key, p, s);
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] p = new Node[SCALE + 1], s = new Node[SCALE + 1];
        while (true) {
            if (!find(key, p, s))
                return false;
            Node n = s[0];
            // Mark upper levels
            for (int i = n.next.length - 1; i >= 1; i--) {
                boolean[] m = { false };
                Node sc = n.next[i].get(m);
                while (!m[0]) {
                    n.next[i].compareAndSet(sc, sc, false, true);
                    sc = n.next[i].get(m);
                }
            }
            boolean[] m = { false };
            Node sc = n.next[0].get(m);
            while (true) {
                if (n.next[0].compareAndSet(sc, sc, false, true)) {
                    // Node has been marked
                    find(key, p, s);
                    return true;
                }
                sc = n.next[0].get(m);
                if (m[0])
                    return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] m = { false };
        Node pr = head;
        for (int i = SCALE; i >= 0; i--) {
            Node cu = pr.next[i].getReference();
            while (true) {
                Node sc = cu.next[i].get(m);
                while (m[0]) {
                    cu = sc;
                    sc = cu.next[i].get(m);
                }
                if (cu.val < key) {
                    pr = cu;
                    cu = sc;
                } else
                    break;
            }
        }
        Node cu = pr.next[0].getReference();
        cu.next[0].get(m);
        return cu.val == key && !m[0];
    }

    private int rnd() {
        int l = 0;
        while (l < SCALE && r.nextInt(2) == 0)
            l++;
        return l;
    }
}
