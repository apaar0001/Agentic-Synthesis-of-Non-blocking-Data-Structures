package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicInteger;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final int MIN_LEVEL = 1;

    private Node head;
    private AtomicInteger level;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        level = new AtomicInteger(MIN_LEVEL);
    }

    @Override
    public boolean add(int key) {
        int[] next = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];

        while (true) {
            if (!find(key, next, preds, marked)) {
                int newLevel = randomLevel();
                if (newLevel > level.get()) {
                    while (level.get() < newLevel) {
                        if (level.compareAndSet(level.get(), newLevel)) break;
                    }
                }

                Node newNode = new Node(key, newLevel);
                if (insertNode(newNode, next, preds, marked)) return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int[] next = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];

        while (true) {
            Node node = find(key, next, preds, marked);
            if (node == null) return false;

            if (node.marked.get()) return false;

            if (node.marked.compareAndSet(false, true)) {
                // Node has been marked
                if (physicalRemove(node, next, preds, marked)) return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int[] next = new int[MAX_LEVEL];
        Node[] preds = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];

        return find(key, next, preds, marked) != null;
    }

    private int randomLevel() {
        int level = MIN_LEVEL;
        while ((level < MAX_LEVEL) && (Math.random() < 0.5)) level++;
        return level;
    }

    private boolean find(int key, int[] next, Node[] preds, boolean[] marked) {
        int i = level.get() - 1;
        Node curr = head;

        while (i >= 0) {
            while (true) {
                Node nextNode = curr.next[i].getReference();
                if (nextNode == null) break;

                if (nextNode.marked.get()) {
                    if (curr.next[i].compareAndSet(nextNode, nextNode.next[i].getReference())) {
                        continue;
                    }
                } else {
                    if (nextNode.key >= key) break;
                    curr = nextNode;
                }
            }

            preds[i] = curr;
            next[i] = curr.next[i].getReference();
            marked[i] = curr.marked.get();
            i--;
        }

        return next[0] != null && next[0].key == key;
    }

    private boolean insertNode(Node newNode, int[] next, Node[] preds, boolean[] marked) {
        for (int i = 0; i < newNode.level; i++) {
            newNode.next[i].set(preds[i].next[i].getReference());
        }

        for (int i = 0; i < newNode.level; i++) {
            while (true) {
                if (preds[i].next[i].compareAndSet(next[i], newNode)) break;
                find(newNode.key, next, preds, marked);
            }
        }

        return true;
    }

    private boolean physicalRemove(Node node, int[] next, Node[] preds, boolean[] marked) {
        for (int i = 0; i < node.level; i++) {
            while (true) {
                if (preds[i].next[i].compareAndSet(node, node.next[i].getReference())) break;
                find(node.key, next, preds, marked);
            }
        }

        return true;
    }

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] next;
        AtomicInteger marked;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            next = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
            }
            marked = new AtomicInteger(0);
        }
    }
}