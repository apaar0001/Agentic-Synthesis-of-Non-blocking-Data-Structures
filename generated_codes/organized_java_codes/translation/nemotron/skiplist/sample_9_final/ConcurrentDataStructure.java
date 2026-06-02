package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.forward[i] = new AtomicMarkableReference<>(tail, false);
            tail.forward[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            find(key, pred, succ);
            Node succ0 = succ[0].getReference();
            if (succ0 != null && succ0.key == key && !succ[0].isMarked()) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            boolean inserted = true;
            for (int level = 0; level <= newLevel; level++) {
                while (!pred[level].forward[level].compareAndSet(
                        succ[level].getReference(), newNode, false, false)) {
                    inserted = false;
                    break;
                }
                if (!inserted) break;
            }
            if (!inserted) {
                continue;
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            find(key, pred, succ);
            Node node = succ[0].getReference();
            if (node == null || node.key != key) {
                return false;
            }
            if (!pred[0].forward[0].compareAndSet(node, node, false, true)) {
                continue;
            }
            // Node has been marked
            for (int level = 1; level <= node.level; level++) {
                while (!pred[level].forward[level].compareAndSet(
                        node, node, false, true)) {
                    break;
                }
            }
            for (int level = 0; level <= node.level; level++) {
                pred[level].forward[level].compareAndSet(
                        node, node.forward[level].getReference(), true, false);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] pred = new Node[MAX_LEVEL + 1];
        Node[] succ = new Node[MAX_LEVEL + 1];
        find(key, pred, succ);
        Node node = succ[0].getReference();
        return node != null && node.key == key && !succ[0].isMarked();
    }

    private void find(int key, Node[] pred, Node[] succ) {
        int level = MAX_LEVEL;
        Node curr = head;
        outer:
        while (true) {
            Node next = curr.forward[level].getReference();
            while (true) {
                if (next == null) {
                    break;
                }
                boolean marked = curr.forward[level].isMarked();
                if (marked) {
                    Node nextNext = next.forward[level].getReference();
                    if (!curr.forward[level].compareAndSet(next, nextNext, false, false)) {
                        continue outer;
                    }
                    next = curr.forward[level].getReference();
                    continue;
                }
                if (next.key < key) {
                    curr = next;
                    next = curr.forward[level].getReference();
                    continue;
                }
                break;
            }
            pred[level] = curr;
            succ[level] = curr.forward[level].getReference();
            if (level == 0) {
                break;
            }
            level--;
        }
    }

    private int randomLevel() {
        int lvl = 0;
        while (ThreadLocalRandom.current().nextBoolean() && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private static class Node {
        final int key;
        int level;
        final AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}