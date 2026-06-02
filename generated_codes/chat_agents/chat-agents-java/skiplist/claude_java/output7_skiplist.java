package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final double PROBABILITY = 0.5;

    private static class Node {
        final int key;
        final int level;
        final AtomicMarkableReference<Node>[] forward;

        @SuppressWarnings("unchecked")
        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    private final Node head;
    private final Node tail;
    private final java.util.concurrent.atomic.AtomicInteger randomSeed;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.forward[i] = new AtomicMarkableReference<>(tail, false);
        }
        randomSeed = new java.util.concurrent.atomic.AtomicInteger(0x12345678);
    }

    private int randomLevel() {
        int level = 0;
        int seed;
        int next;
        do {
            seed = randomSeed.get();
            next = seed * 1664525 + 1013904223;
        } while (!randomSeed.compareAndSet(seed, next));
        int bits = next;
        while ((bits & 1) == 1 && level < MAX_LEVEL) {
            level++;
            bits >>= 1;
        }
        return level;
    }

    @SuppressWarnings("unchecked")
    private Node[] find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean snip;
        Node pred, curr, succ;

        retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                curr = pred.forward[level].getReference();
                while (true) {
                    succ = curr.forward[level].get(marked);
                    while (marked[0]) {
                        snip = pred.forward[level].compareAndSet(curr, succ, false, false);
                        if (!snip) continue retry;
                        curr = pred.forward[level].getReference();
                        succ = curr.forward[level].get(marked);
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return succs;
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];

        while (true) {
            find(key, preds, succs);
            if (succs[0] != null && succs[0].key == key) {
                return false;
            }
            Node newNode = new Node(key, topLevel);
            for (int level = 0; level <= topLevel; level++) {
                newNode.forward[level] = new AtomicMarkableReference<>(succs[level], false);
            }
            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.forward[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }
            for (int level = 1; level <= topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.forward[level].compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        boolean[] marked = {false};

        while (true) {
            find(key, preds, succs);
            Node target = succs[0];
            if (target == null || target.key != key) {
                return false;
            }
            int topLevel = target.level;
            for (int level = topLevel; level >= 1; level--) {
                Node succ = target.forward[level].get(marked);
                while (!marked[0]) {
                    target.forward[level].compareAndSet(succ, succ, false, true);
                    succ = target.forward[level].get(marked);
                }
            }
            Node succ = target.forward[0].get(marked);
            while (true) {
                boolean markedIt = target.forward[0].compareAndSet(succ, succ, false, true);
                if (markedIt) {
                    // Node has been marked
                    find(key, preds, succs);
                    return true;
                }
                succ = target.forward[0].get(marked);
                if (marked[0]) {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = {false};
        Node curr = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node next = curr.forward[level].getReference();
            while (next.key < key) {
                curr = next;
                next = curr.forward[level].getReference();
            }
        }
        curr = curr.forward[0].getReference();
        curr.forward[0].get(marked);
        return curr.key == key && !marked[0];
    }
}