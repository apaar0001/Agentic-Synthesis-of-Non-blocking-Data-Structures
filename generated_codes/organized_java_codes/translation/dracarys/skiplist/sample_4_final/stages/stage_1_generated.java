package com.example.Sets;

import com.example.utils.SetADT;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private static final int MIN_LEVEL = 1;
    private static final Random random = new Random();

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final AtomicInteger level;

        Node(int key, int level) {
            this.key = key;
            this.next = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
            this.level = new AtomicInteger(level);
        }
    }

    private final Node head;
    private final AtomicInteger tailLevel;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tailLevel = new AtomicInteger(MIN_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.next[i].set(new Node(Integer.MAX_VALUE, MAX_LEVEL), false);
        }
    }

    private int randomLevel() {
        int level = 1;
        while (random.nextBoolean() && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private Node find(int key, Node[] preds, Node[] succs) {
        int level = tailLevel.get();
        Node pred = head;
        for (int i = level - 1; i >= 0; i--) {
            Node succ = pred.next[i].getReference();
            while (succ != null && succ.key < key) {
                pred = succ;
                succ = pred.next[i].getReference();
            }
            preds[i] = pred;
            succs[i] = succ;
            if (succ != null && succ.key == key) {
                level = i + 1;
            }
        }
        return succ;
    }

    @Override
    public boolean add(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        while (true) {
            Node succ = find(key, preds, succs);
            if (succ != null && succ.key == key) {
                return false;
            }
            int level = randomLevel();
            if (level > tailLevel.get()) {
                tailLevel.set(level);
            }
            Node newNode = new Node(key, level);
            for (int i = 0; i < level; i++) {
                newNode.next[i].set(succs[i], false);
            }
            if (level > 1) {
                for (int i = level - 1; i >= 0; i--) {
                    while (!preds[i].next[i].compareAndSet(succs[i], newNode, false, false)) {
                        find(key, preds, succs);
                    }
                }
            } else {
                while (!preds[0].next[0].compareAndSet(succs[0], newNode, false, false)) {
                    find(key, preds, succs);
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        while (true) {
            Node succ = find(key, preds, succs);
            if (succ == null || succ.key != key) {
                return false;
            }
            for (int i = 0; i < succ.level.get(); i++) {
                if (!succ.next[i].getReference().equals(succs[i])) {
                    find(key, preds, succs);
                    break;
                }
            }
            if (succ.next[0].getReference().equals(succs[0])) {
                boolean marked = succ.next[0].attemptMark(succs[0], true);
                if (marked) {
                    // Node has been marked
                    for (int i = 0; i < succ.level.get(); i++) {
                        preds[i].next[i].compareAndSet(succ, succs[i], false, false);
                    }
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        Node succ = find(key, preds, succs);
        return succ != null && succ.key == key && !succ.next[0].isMarked();
    }
}