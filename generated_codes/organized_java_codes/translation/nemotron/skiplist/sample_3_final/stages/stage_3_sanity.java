package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    private static final int MAX_LEVEL = 16;
    private static final double PROBABILITY = 0.5;
    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.forward[i].set(tail, false);
        }
    }

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] f = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                f[i] = new AtomicMarkableReference<>(null, false);
            }
            this.forward = f;
        }
    }

    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private Node[] find(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node pred = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node curr = pred.forward[level].getReference();
            while (true) {
                if (curr == null) break;
                boolean marked = pred.forward[level].isMarked();
                if (marked) {
                    Node next = curr.forward[level].getReference();
                    if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                        curr = next;
                        continue;
                    } else {
                        curr = pred.forward[level].getReference();
                        continue;
                    }
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = pred.forward[level].getReference();
                } else {
                    break;
                }
            }
            preds[level] = pred;
        }
        return preds;
    }

    private Node findPredecessorFromHead(int key, int level) {
        Node pred = head;
        Node curr = pred.forward[level].getReference();
        while (true) {
            if (curr == null) break;
            boolean marked = pred.forward[level].isMarked();
            if (marked) {
                Node next = curr.forward[level].getReference();
                if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                    curr = next;
                    continue;
                } else {
                    curr = pred.forward[level].getReference();
                    continue;
                }
            }
            if (curr.key < key) {
                pred = curr;
                curr = pred.forward[level].getReference();
            } else {
                break;
            }
        }
        return pred;
    }

    private void helpRemove(Node node) {
        for (int level = 0; level <= node.level; level++) {
            while (true) {
                Node[] preds = find(node.key);
                Node pred = preds[level];
                AtomicMarkableReference<Node> predRef = pred.forward[level];
                Node succ = node.forward[level].getReference();
                if (predRef.compareAndSet(node, succ, false, false)) {
                    break;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = find(key);
        Node curr = preds[0].forward[0].getReference();
        return curr != null && curr.key == key && !curr.forward[0].isMarked();
    }

    @Override
    public boolean add(int key) {
        outer:
        while (true) {
            Node[] preds = find(key);
            Node succ = preds[0].forward[0].getReference();
            if (succ != null && succ.key == key && !succ.forward[0].isMarked()) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            for (int level = 0; level <= newLevel; level++) {
                while (true) {
                    Node pred = preds[level];
                    AtomicMarkableReference<Node> predRef = pred.forward[level];
                    Node expected = pred.forward[level].getReference();
                    if (predRef.compareAndSet(expected, newNode, false, false)) {
                        break;
                    } else {
                        continue outer;
                    }
                }
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        outer:
        while (true) {
            Node[] preds = find(key);
            Node node