package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

public class ConcurrentDataStructure implements SetADT {

    private static final int MAX_LEVEL = 16;
    private final Node head;
    private final Node tail;
    private final Random rand;

    public ConcurrentDataStructure() {
        this.rand = new Random();
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        this.tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.forward[i] = new AtomicMarkableReference<>(tail, false);
            tail.forward[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            Node succ = find(key, preds, succs);
            if (succ != null && succ.key == key && !succ.forward[0].isMarked()) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            for (int i = 0; i < newLevel; i++) {
                newNode.forward[i] = new AtomicMarkableReference<>(succs[i], false);
            }
            boolean inserted = true;
            for (int i = 0; i < newLevel; i++) {
                if (!preds[i].forward[i].compareAndSet(succs[i], newNode, false, false)) {
                    inserted = false;
                    break;
                }
            }
            if (inserted) {
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        outer:
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            Node succ = find(key, preds, succs);
            if (succ == null || succ.key != key || succ.forward[0].isMarked()) {
                return false;
            }
            Node nodeToRemove = succ;
            boolean marked = false;
            while (!marked) {
                Node pred0 = preds[0];
                marked = pred0.forward[0].compareAndSet(nodeToRemove, nodeToRemove, false, true);
                if (marked) {
                    // Node has been marked
                    break;
                } else {
                    break outer;
                }
            }
            for (int level = 0; level < nodeToRemove.forward.length; level++) {
                Node succNode = nodeToRemove.forward[level].getReference();
                while (true) {
                    Node predNode = preds[level];
                    boolean success = predNode.forward[level].compareAndSet(
                            nodeToRemove, succNode,
                            predNode.forward[level].isMarked(), false);
                    if (success) {
                        break;
                    } else {
                        if (predNode.forward[level].isMarked()) {
                            Node markedNode = predNode.forward[level].getReference();
                            Node next = markedNode.forward[level].getReference();
                            if (predNode.forward[level].compareAndSet(markedNode, next, true, false)) {
                                continue;
                            } else {
                                break outer;
                            }
                        } else {
                            break outer;
                        }
                    }
                }
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        Node succ = find(key, preds, succs);
        return succ != null && succ.key == key && !succ.forward[0].isMarked();
    }

    private Node find(int key, Node[] preds, Node[] succs) {
        Node pred = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            Node curr = pred.forward[level].getReference();
            while (true) {
                Node next = curr.forward[level].getReference();
                boolean marked = pred.forward[level].isMarked();
                if (marked) {
                    if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                        curr = next;
                        continue;
                    } else {
                        break;
                    }
                }
                if (next == null || next.key >= key) {
                    break;
                }
                pred = curr;
                curr = next;
            }
            preds[level] = pred;
            succs[level] = curr;
        }
        return succs[0];
    }

    private int randomLevel() {
        int level = 1;
        while ((rand.nextInt(2) == 0) && (level < MAX_LEVEL)) {
            level++;
        }
        return level;
    }

    @SuppressWarnings("unchecked")
    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.forward = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}