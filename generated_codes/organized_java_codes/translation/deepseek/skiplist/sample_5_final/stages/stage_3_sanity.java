package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final AtomicInteger size;

    private static final class Node {
        final int key;
        final AtomicMarkableReference<Node>[] forward;
        final int topLevel;

        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.forward = new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.forward[i].set(tail, false);
        }
        size = new AtomicInteger(0);
    }

    private int randomLevel() {
        int level = 1;
        while (ThreadLocalRandom.current().nextDouble() < 0.5 && level < MAX_LEVEL) {
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
            if (find(key, preds, succs)) {
                return false;
            }

            Node newNode = new Node(key, topLevel);
            for (int level = 0; level < topLevel; level++) {
                Node succ = succs[level];
                newNode.forward[level].set(succ, false);
            }

            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.forward[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }

            for (int level = 1; level < topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.forward[level].compareAndSet(succ, newNode, false, false)) {
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
            if (!find(key, preds, succs)) {
                return false;
            }

            victim = succs[0];
            if (victim.key != key) {
                return false;
            }

            for (int level = victim.topLevel - 1; level >= 1; level--) {
                boolean[] marked = {false};
                Node succ = victim.forward[level].get(marked);
                while (!marked[0]) {
                    victim.forward[level].compareAndSet(succ, succ, false, true);
                    succ = victim.forward[level].get(marked);
                }
            }

            boolean[] marked = {false};
            Node succ = victim.forward[0].get(marked);
            while (true) {
                boolean iMarkedIt = victim.forward[0].compareAndSet(succ, succ, false, true);
                succ = succs[0].forward[0].get(marked);
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
        Node curr = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.forward[level].get(marked);
                while (marked[0]) {
                    succ = succ.forward[level].get(marked);
                }
                if (succ.key < key) {
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        boolean[] marked = {false};
        Node succ = curr.forward[0].get(marked);
        return succ.key == key && !marked[0];
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean snip;
        Node pred, curr, succ;

        retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL - 1; level >= 0; level--) {
                curr = pred.forward[level].getReference();
                while (true) {
                    succ = curr.forward[level].get(marked);
                    while (marked[0]) {
                        snip = pred.forward[level].compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry;
                        }
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
            return curr.key == key;
        }
    }
}