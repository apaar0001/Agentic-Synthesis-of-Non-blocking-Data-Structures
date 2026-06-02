package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final Random random;
    private final AtomicInteger size;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final int topLevel;

        @SuppressWarnings("unchecked")
        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
        }
        this.random = new Random();
        this.size = new AtomicInteger(0);
    }

    private int randomLevel() {
        int level = 1;
        while (level < MAX_LEVEL && random.nextDouble() < 0.5) {
            level++;
        }
        return level;
    }

    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        while (true) {
            boolean found = find(key, preds, succs);
            if (found) {
                return false;
            }

            Node newNode = new Node(key, topLevel);
            for (int level = 0; level < topLevel; level++) {
                Node succ = succs[level];
                newNode.next[level].set(succ, false);
            }

            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.next[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }

            for (int level = 1; level < topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.next[level].compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            size.incrementAndGet();
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        Node victim;

        while (true) {
            boolean found = find(key, preds, succs);
            if (!found) {
                return false;
            }

            victim = succs[0];
            for (int level = victim.topLevel - 1; level >= 1; level--) {
                boolean[] marked = {false};
                Node next = victim.next[level].get(marked);
                while (!marked[0]) {
                    victim.next[level].attemptMark(next, true);
                    next = victim.next[level].get(marked);
                }
            }

            boolean[] marked = {false};
            Node next = victim.next[0].get(marked);
            while (true) {
                boolean iMarkedIt = victim.next[0].compareAndSet(next, next, false, true);
                // Node has been marked
                if (iMarkedIt) {
                    find(key, preds, succs);
                    size.decrementAndGet();
                    return true;
                }
                next = victim.next[0].get(marked);
                if (marked[0]) {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node pred = head;
        Node curr = null;
        boolean[] marked = {false};

        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            curr = pred.next[level].getReference();
            while (true) {
                Node succ = curr.next[level].get(marked);
                while (marked[0]) {
                    curr = succ;
                    succ = curr.next[level].get(marked);
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr.key == key;
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        boolean snip;

        retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL - 1; level >= 0; level--) {
                curr = pred.next[level].getReference();
                while (true) {
                    succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        snip = pred.next[level].compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry;
                        }
                        curr = pred.next[level].getReference();
                        succ = curr.next[level].get(marked);
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
            return curr.key == key;
        }
    }
}