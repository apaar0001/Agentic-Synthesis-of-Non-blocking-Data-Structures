package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final int MIN_LEVEL = 1;

    private Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.next[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    @Override
    public boolean add(int key) {
        int[] next = new int[MAX_LEVEL];
        int[] prev = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        while (true) {
            if (!find(key, next, prev, preds, succs)) {
                int level = randomLevel();
                Node node = new Node(key, level);

                for (int i = 0; i < level; i++) {
                    node.next[i] = new AtomicMarkableReference<>(succs[i], false);
                }

                if (cas(prev, next, node, preds, succs)) {
                    return true;
                }
            } else {
                return false;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int[] next = new int[MAX_LEVEL];
        int[] prev = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        while (true) {
            if (find(key, next, prev, preds, succs)) {
                Node node = preds[0];
                if (node.key == key) {
                    if (mark(node, succs)) {
                        if (cas(prev, next, node, preds, succs)) {
                            return true;
                        }
                    }
                } else {
                    return false;
                }
            } else {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int[] next = new int[MAX_LEVEL];
        int[] prev = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        if (find(key, next, prev, preds, succs)) {
            Node node = preds[0];
            return node.key == key && !node.next[0].getReference().isMarked();
        } else {
            return false;
        }
    }

    private boolean cas(int[] prev, int[] next, Node node, Node[] preds, Node[] succs) {
        for (int i = 0; i < node.level; i++) {
            if (preds[i].next[i].getReference().isMarked() || preds[i].next[i].getReference() != succs[i]) {
                return false;
            }
        }

        for (int i = 0; i < node.level; i++) {
            if (!preds[i].next[i].compareAndSet(succs[i], node, false, false)) {
                return false;
            }
        }

        return true;
    }

    private boolean mark(Node node, Node[] succs) {
        for (int i = 0; i < node.level; i++) {
            if (node.next[i].getReference().isMarked() || node.next[i].getReference() != succs[i]) {
                return false;
            }
        }

        for (int i = 0; i < node.level; i++) {
            if (!node.next[i].compareAndSet(succs[i], succs[i], false, true)) {
                return false;
            }
        }

        // Node has been marked
        return true;
    }

    private boolean find(int key, int[] next, int[] prev, Node[] preds, Node[] succs) {
        int l = MAX_LEVEL - 1;
        Node pred = head;

        while (true) {
            Node succ = pred.next[l].getReference();
            while (succ != null && (succ.key < key || (succ.key == key && succ.next[0].getReference().isMarked()))) {
                pred = succ;
                succ = pred.next[l].getReference();
            }

            if (succ != null && succ.key == key) {
                preds[l] = pred;
                succs[l] = succ;
                l--;
                if (l < 0) {
                    return true;
                }
            } else {
                if (pred.key == key) {
                    preds[l] = pred;
                    succs[l] = pred.next[l].getReference();
                    l--;
                    if (l < 0) {
                        return true;
                    }
                } else {
                    return false;
                }
            }
        }
    }

    private int randomLevel() {
        int level = MIN_LEVEL;
        while (Math.random() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] next;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            next = new AtomicMarkableReference[level];
        }
    }
}