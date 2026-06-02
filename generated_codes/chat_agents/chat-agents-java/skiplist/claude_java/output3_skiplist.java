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
    private final java.util.concurrent.atomic.AtomicInteger maxLevel;
    private final java.util.Random random;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.forward[i].set(tail, false);
        }
        maxLevel = new java.util.concurrent.atomic.AtomicInteger(0);
        random = new java.util.Random();
    }

    private int randomLevel() {
        int lvl = 0;
        while (lvl < MAX_LEVEL && random.nextDouble() < PROBABILITY) {
            lvl++;
        }
        return lvl;
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
        N