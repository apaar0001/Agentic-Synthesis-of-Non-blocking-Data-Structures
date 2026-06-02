package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double P = 0.5;

    private final Node header;
    private final AtomicInteger level;

    public ConcurrentDataStructure() {
        level = new AtomicInteger(1);
        header = new Node(Integer.MIN_VALUE, MAX_LEVEL);
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            if (find(key, preds, succs)) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            for (int i = 1; i <= newLevel; i++) {
                newNode.forward[i].set(succs[i], false);
            }
            boolean ok = true;
            for (int i = 1; i <= newLevel; i++) {
                if (!preds[i].forward[i].compareAndSet(succs[i], newNode, false, false)) {
                    ok = false;
                    break;
                }
            }
            if (!ok) {
                continue;
            }
            while (true) {
                int cur = level.get();
                if (newLevel <= cur) {
                    break;
                }
                if (level.compareAndSet(cur, newLevel)) {
                    break;
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        outer: while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            if (!find(key, preds, succs)) {
                return false;
            }
            Node node = succs[1];
            if (!preds[1].forward[1].compareAndSet(node, node, false, true)) {
                return false;
            }
            // Node has been marked
            int maxLevel = node.forward.length - 1;
            for (int i = 1; i <= maxLevel; i++) {
                Node succ = node.forward[i].getReference();
                while (!preds[i].forward[i].compareAndSet(node, succ, true, false)) {
                    if (preds[i].forward[i].getReference() != node) {
                        continue outer;
                    }
                }
            }
            while (level.get() > 1 && header.forward[level.get()].getReference() == null) {
                int l = level.get();
                level.compareAndSet(l, l - 1);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        return find(key, preds, succs);
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        outer: while (true) {
            Node pred = header;
            int lvl = level.get();
            for (int i = lvl; i >= 1; i--) {
                Node curr = pred.forward[i].getReference();
                while (true) {
                    boolean marked = pred.forward[i].isMarked();
                    if (marked) {
                        Node succ = curr.forward[i].getReference();
                        if (pred.forward[i].compareAndSet(curr, succ, true, false)) {
                            curr = pred.forward[i].getReference();
                            continue;
                        } else {
                            continue outer;
                        }
                    }
                    if (curr == null) {
                        preds[i] = pred;
                        succs[i] = null;
                        break;
                    }
                    if (curr.key >= key) {
                        preds[i] = pred;
                        succs[i] = curr;
                        break;
                    }
                    pred = curr;
                    curr = pred.forward[i].getReference();
                }
            }
            Node succ = succs[1];
            return succ != null && succ.key == key && !preds[1].forward[1].isMarked();
        }
    }

    private int randomLevel() {
        int lvl = 1;
        while (ThreadLocalRandom.current().nextDouble() < P && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private static class Node {
        int key;
        final AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] arr = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                arr[i] = new AtomicMarkableReference<>(null, false);
            }
            this.forward = arr;
        }
    }
}