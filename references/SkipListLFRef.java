package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

/**
 * Lock-free skip list — Herlihy-Lev-Luchangco-Moir (HLLM) algorithm.
 *
 * Ground-truth reference for LockFreeBench annotation evaluation.
 *
 * Progress guarantee : LOCK_FREE
 * Algorithm : HLLM skip list (Art of Multiprocessor Programming Ch.14)
 * ABA protection : AtomicMarkableReference per level (mark = logically deleted)
 * Linearization points:
 * add() — successful CAS at level 0 linking new node [FIXED LP]
 * remove() — successful CAS marking bottom-level next [FIXED LP]
 * contains() — wait-free read: unmarked node found at key [FIXED LP]
 */
public class SkipListLFRef implements SetADT {

    private static final int MAX_LEVEL = 32;

    // ---- Node -------------------------------------------------------------

    private static final class Node {
        final int key;
        final int height; // number of levels this node spans
        @SuppressWarnings("unchecked")
        final AtomicMarkableReference<Node>[] next;

        /** Internal constructor (sentinels + skip nodes) */
        @SuppressWarnings("unchecked")
        Node(int key, int height) {
            this.key = key;
            this.height = height;
            this.next = new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++)
                this.next[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;

    public SkipListLFRef() {
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++)
            head.next[i].set(tail, false);
    }

    // ---- Random height (geometric distribution) ---------------------------

    private static int randomHeight() {
        int h = 1;
        while (h < MAX_LEVEL && ThreadLocalRandom.current().nextBoolean())
            h++;
        return h;
    }

    // ---- find — returns predecessor/successor arrays ----------------------

    @SuppressWarnings("unchecked")
    private Node[] find(int key, Node[] preds, Node[] succs) {
        retry: while (true) {
            Node pred = head;
            for (int level = MAX_LEVEL - 1; level >= 0; level--) {
                boolean[] marked = { false };
                Node curr = pred.next[level].get(marked);
                while (true) {
                    Node succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        // physically remove marked node at this level
                        boolean snip = pred.next[level].compareAndSet(curr, succ, false, false);
                        if (!snip)
                            continue retry;
                        curr = succ;
                        succ = curr.next[level].get(marked);
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else
                        break;
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return preds;
        }
    }

    // ---- SetADT operations ------------------------------------------------

    @Override
    public boolean add(int key) {
        int height = randomHeight();
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        while (true) {
            find(key, preds, succs);
            if (succs[0].key == key)
                return false; // duplicate

            Node node = new Node(key, height);
            for (int i = 0; i < height; i++)
                node.next[i].set(succs[i], false);

            // LINEARIZATION POINT: CAS at level 0 links node into base list
            Node pred = preds[0], succ = succs[0];
            if (!pred.next[0].compareAndSet(succ, node, false, false))
                continue;

            // Link upper levels (failures are harmless — find() will repair)
            for (int i = 1; i < height; i++) {
                while (true) {
                    pred = preds[i];
                    succ = succs[i];
                    if (pred.next[i].compareAndSet(succ, node, false, false))
                        break;
                    find(key, preds, succs);
                    if (succs[i].key == key) {
                        node.next[i].set(succs[i], false);
                    }
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        find(key, preds, succs);
        Node victim = succs[0];
        if (victim.key != key)
            return false; // not present

        // Mark all levels top-down (logical deletion)
        for (int i = victim.height - 1; i > 0; i--) {
            boolean[] marked = { false };
            Node succ = victim.next[i].get(marked);
            while (!marked[0]) {
                victim.next[i].compareAndSet(succ, succ, false, true);
                succ = victim.next[i].get(marked);
            }
        }

        // LINEARIZATION POINT: mark bottom level
        boolean[] marked0 = { false };
        while (true) {
            Node succ = victim.next[0].get(marked0);
            if (marked0[0])
                return false; // already removed by another thread
            if (victim.next[0].compareAndSet(succ, succ, false, true)) {
                find(key, preds, succs); // physical cleanup
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        // Wait-free search (no CAS, no cleanup)
        boolean[] marked = { false };
        Node curr = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            Node next = curr.next[level].get(marked);
            while (next.key < key) {
                curr = next;
                next = curr.next[level].get(marked);
            }
        }
        // LINEARIZATION POINT: read of curr's successor at level 0
        curr = curr.next[0].get(marked);
        return curr.key == key && !marked[0];
    }
}
