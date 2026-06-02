package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.ThreadLocalRandom;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final AtomicInteger size;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final int topLevel;

        @SuppressWarnings("unchecked")
        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.next = (AtomicMarkableReference<Node>[]) new AtomicMarkableReference[height + 1];
            for (int i = 0; i <= height; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
        }
        this.size = new AtomicInteger(0);
    }

    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];

        while (true) {
            boolean found = find(key, preds, succs);
            if (found) {
                return false;
            }

            Node newNode = new Node(key, topLevel);
            for (int level = 0; level <= topLevel; level++) {
                newNode.next[level].set(succs[level], false);
            }

            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.next[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }

            for (int level = 1; level <= topLevel; level++) {
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
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        Node succ;

        while (true) {
            boolean found = find(key, preds, succs);
            if (!found) {
                return false;
            }

            Node nodeToRemove = succs[0];
            for (int level = nodeToRemove.topLevel; level >= 1; level--) {
                boolean[] marked = {false};
                succ = nodeToRemove.next[level].get(marked);
                while (!marked[0]) {
                    nodeToRemove.next[level].compareAndSet(succ, succ, false, true);
                    succ = nodeToRemove.next[level].get(marked);
                }
            }

            boolean[] marked = {false};
            succ = nodeToRemove.next[0].get(marked);
            while (true) {
                boolean iMarkedIt = nodeToRemove.next[0].compareAndSet(succ, succ, false, true);
                succ = succs[0].next[0].get(marked);
                if (iMarkedIt) {
                    // Node has been marked
                    find(key, preds, succs);
                    size.decrementAndGet();
                    return true;
                } else if (marked[0]) {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node pred = head;
        Node curr = null;
        Node succ;
        boolean[] marked = {false};
        int level = MAX_LEVEL;

        while (level >= 0) {
            curr = pred.next[level].getReference();
            while (true) {
                succ = curr.next[level].get(marked);
                while (marked[0]) {
                    curr = curr.next[level].getReference();
                    succ = curr.next[level].get(marked);
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
            level--;
        }
        return curr.key == key;
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        Node pred = head;
        Node curr = null;
        Node succ;
        boolean[] marked = {false};
        boolean snip;

        retry: while (true) {
            for (int level = MAX_LEVEL; level >= 0; level--) {
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