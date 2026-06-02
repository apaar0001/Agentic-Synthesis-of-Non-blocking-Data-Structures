package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

/**
 * Lock-free Skip List — Sentinel Node variant.
 * Names: Node / forward[] / val / MAX_LVL=31 / search() / Pair l/r
 *
 * Distinguishing characteristics:
 * - Traversal helper: Pair (left/right) instead of Window/preds+succs arrays
 * - Forward pointer array: forward[] instead of next[]
 * - Level constant: MAX_LVL=31 (not 32)
 * - Traversal method: search() instead of find()
 */
public class SkipListSentinelNode implements SetADT {

    private static final int MAX_LVL = 31;

    private static class Node {
        final int val;
        @SuppressWarnings("unchecked")
        final AtomicMarkableReference<Node>[] forward;

        @SuppressWarnings("unchecked")
        Node(int v, int l) {
            this.val = v;
            this.forward = new AtomicMarkableReference[l + 1];
            for (int i = 0; i <= l; i++)
                forward[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;
    private final Random random = new Random();

    public SkipListSentinelNode() {
        head = new Node(Integer.MIN_VALUE, MAX_LVL);
        tail = new Node(Integer.MAX_VALUE, MAX_LVL);
        for (int i = 0; i <= MAX_LVL; i++)
            head.forward[i].set(tail, false);
    }

    private boolean search(int key, Node[] p, Node[] s) {
        boolean[] m = { false };
        retry: while (true) {
            Node currP = head;
            for (int i = MAX_LVL; i >= 0; i--) {
                Node curr = currP.forward[i].getReference();
                while (true) {
                    Node next = curr.forward[i].get(m);
                    while (m[0]) {
                        if (!currP.forward[i].compareAndSet(curr, next, false, false))
                            continue retry;
                        curr = next;
                        next = curr.forward[i].get(m);
                    }
                    if (curr.val < key) {
                        currP = curr;
                        curr = next;
                    } else
                        break;
                }
                p[i] = currP;
                s[i] = curr;
            }
            return s[0].val == key;
        }
    }

    @Override
    public boolean add(int key) {
        int l = rndLvl();
        Node[] p = new Node[MAX_LVL + 1], s = new Node[MAX_LVL + 1];
        while (true) {
            if (search(key, p, s))
                return false;
            Node newNode = new Node(key, l);
            for (int i = 0; i <= l; i++)
                newNode.forward[i].set(s[i], false);
            if (!p[0].forward[0].compareAndSet(s[0], newNode, false, false))
                continue;
            for (int i = 1; i <= l; i++) {
                while (!p[i].forward[i].compareAndSet(s[i], newNode, false, false))
                    search(key, p, s);
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] p = new Node[MAX_LVL + 1], s = new Node[MAX_LVL + 1];
        while (true) {
            if (!search(key, p, s))
                return false;
            Node node = s[0];
            // Mark upper levels first
            for (int i = node.forward.length - 1; i >= 1; i--) {
                boolean[] m = { false };
                Node next = node.forward[i].get(m);
                while (!m[0]) {
                    node.forward[i].compareAndSet(next, next, false, true);
                    next = node.forward[i].get(m);
                }
            }
            // Linearization: mark level-0
            boolean[] m = { false };
            Node next = node.forward[0].get(m);
            while (true) {
                if (node.forward[0].compareAndSet(next, next, false, true)) {
                    // Node has been marked
                    search(key, p, s);
                    return true;
                }
                next = node.forward[0].get(m);
                if (m[0])
                    return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] m = { false };
        Node cP = head;
        for (int i = MAX_LVL; i >= 0; i--) {
            Node c = cP.forward[i].getReference();
            while (true) {
                Node s = c.forward[i].get(m);
                while (m[0]) {
                    c = s;
                    s = c.forward[i].get(m);
                }
                if (c.val < key) {
                    cP = c;
                    c = s;
                } else
                    break;
            }
        }
        Node c = cP.forward[0].getReference();
        c.forward[0].get(m);
        return c.val == key && !m[0];
    }

    private int rndLvl() {
        int l = 0;
        while (l < MAX_LVL && random.nextInt(2) == 0)
            l++;
        return l;
    }
}
