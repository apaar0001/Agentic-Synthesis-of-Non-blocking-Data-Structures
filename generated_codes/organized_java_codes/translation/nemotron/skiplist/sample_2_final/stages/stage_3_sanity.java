package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final double PROBABILITY = 0.5;

    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        @SuppressWarnings("unchecked")
        AtomicMarkableReference<Node>[] fwd = new AtomicMarkableReference[MAX_LEVEL];
        for (int i = 0; i < MAX_LEVEL; i++) {
            fwd[i] = new AtomicMarkableReference<>(tail, false);
        }
        head.forward = fwd;
        for (int i = 0; i < MAX_LEVEL; i++) {
            tail.forward[i] = new AtomicMarkableReference<>(null, false);
        }
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
            boolean ok = true;
            for (int level = 0; level < topLevel; level++) {
                newNode.forward[level] = new AtomicMarkableReference<>(succs[level], false);
                while (true) {
                    AtomicMarkableReference<Node> predRef = preds[level].forward[level];
                    Node expected = succs[level];
                    if (predRef.compareAndSet(expected, newNode, false, false)) {
                        break;
                    }
                    if (!helpRemoveIfMarked(preds[level], level, expected)) {
                        ok = false;
                        break;
                    }
                }
                if (!ok) break;
            }
            if (ok) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }
            Node node = succs[0];
            if (node.forward[0].compareAndSet(node, node, false, true)) {
                // Node has been marked
                for (int level = 0; level < MAX_LEVEL; level++) {
                    AtomicMarkableReference<Node> predRef = preds[level].forward[level];
                    Node expected = node;
                    while (true) {
                        Node curr = predRef.getReference();
                        if (curr != expected) break;
                        boolean marked = predRef.isMarked();
                        Node next = curr.forward[level].getReference();
                        if (marked) {
                            predRef.compareAndSet(curr, next, false, false);
                            break;
                        }
                        if (predRef.compareAndSet(expected, next, false, false)) {
                            break;
                        }
                    }
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            Node succ = pred.forward[level].getReference();
            while (true) {
                if (succ == null) break;
                boolean marked = pred.forward[level].isMarked();
                Node next = succ.forward[level].getReference();
                if (marked) {
                    pred.forward[level].compareAndSet(succ, next, false, false);
                    succ = pred.forward[level].getReference();
                    continue;
                }
                if (succ.key < key) {
                    pred = succ;
                    succ = pred.forward[level].getReference();
                } else {
                    break;
                }
            }
            if (succ != null && succ.key == key && !succ.forward[0].isMarked()) {
                return true;
            }
        }
        return false;
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        Node pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            Node succ = pred.forward[level].getReference();
            while (true) {
                if (succ == null) break;
                boolean marked = pred.forward[level].isMarked();
                Node next = succ.forward[level].getReference();
                if (marked) {
                    pred.forward[level].compareAndSet(succ, next, false, false);
                    succ = pred.forward[level].getReference();
                    continue;
                }
                if (succ.key < key) {
                    pred = succ;
                    succ = pred.forward[level].getReference();
                } else {
                    break;
                }
            }
            preds[level] = pred;
            succs[level] = succ;
        }
        return succs[0] != null && succs[0].key == key && !succs[0].forward[0].isMarked();
    }

    private boolean helpRemoveIfMarked(Node pred, int level, Node expected) {
        AtomicMarkableReference<Node> ref = pred.forward[level];
        Node curr = ref.getReference();
        if (curr == null) return false;
        boolean marked = ref.isMarked();
        if (!marked) return false;
        Node next = curr.forward[level].getReference();
        return ref.compareAndSet(curr, next, false, false);
    }

    private int randomLevel() {
        int level = 1;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node>[] forward;
        int level;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] f = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                f[i] = new AtomicMarkableReference<>(null, false);
            }
            this.forward = f;
        }
    }
}