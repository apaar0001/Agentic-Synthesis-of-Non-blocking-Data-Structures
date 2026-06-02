package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.Random;

/**
 * Lock-free Skip List — Harris-Michael style.
 * Uses AtomicMarkableReference on every forward pointer at every level.
 * Names: Node / next[] / key / topLevel / MAX_LEVEL=32 / find()
 *
 * remove() two-phase: mark all levels top→1, then CAS level-0 mark.
 * Linearization point: successful level-0 CAS.
 * Lock-freedom: every CAS attempt makes global progress.
 */
public class SkipListHarrisMichael implements SetADT {

    private static final int MAX_LEVEL = 32;

    private static class Node {
        final int key;
        final int topLevel;
        @SuppressWarnings("unchecked")
        final AtomicMarkableReference<Node>[] next;

        @SuppressWarnings("unchecked")
        Node(int key, int level) {
            this.key = key;
            this.topLevel = level;
            this.next = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++)
                next[i] = new AtomicMarkableReference<>(null, false);
        }
    }

    private final Node head;
    private final Node tail;
    private final Random random = new Random();

    public SkipListHarrisMichael() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++)
            head.next[i].set(tail, false);
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = { false };
        retry: while (true) {
            Node pred = head;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                Node curr = pred.next[level].getReference();
                while (true) {
                    Node succ = curr.next[level].get(marked);
                    while (marked[0]) {
                        if (!pred.next[level].compareAndSet(curr, succ, false, false))
                            continue retry;
                        curr = pred.next[level].getReference();
                        succ = curr.next[level].get(marked);
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else
                        break;
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return succs[0].key == key;
        }
    }

    @Override
    public boolean add(int key) {
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL + 1], succs = new Node[MAX_LEVEL + 1];
        while (true) {
            if (find(key, preds, succs))
                return false;
            Node newNode = new Node(key, topLevel);
            for (int i = 0; i <= topLevel; i++)
                newNode.next[i].set(succs[i], false);
            if (!preds[0].next[0].compareAndSet(succs[0], newNode, false, false))
                continue;
            for (int i = 1; i <= topLevel; i++) {
                while (!preds[i].next[i].compareAndSet(succs[i], newNode, false, false))
                    find(key, preds, succs);
            }
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        Node[] preds = new Node[MAX_LEVEL + 1], succs = new Node[MAX_LEVEL + 1];
        while (true) {
            if (!find(key, preds, succs))
                return false;
            Node nodeToRemove = succs[0];
            // Mark all upper levels first (top → 1)
            for (int level = nodeToRemove.topLevel; level >= 1; level--) {
                boolean[] marked = { false };
                Node succ = nodeToRemove.next[level].get(marked);
                while (!marked[0]) {
                    nodeToRemove.next[level].compareAndSet(succ, succ, false, true);
                    succ = nodeToRemove.next[level].get(marked);
                }
            }
            // Linearization point: mark level-0
            boolean[] marked = { false };
            Node succ = nodeToRemove.next[0].get(marked);
            while (true) {
                boolean iMarkedIt = nodeToRemove.next[0].compareAndSet(succ, succ, false, true);
                succ = succs[0].next[0].get(marked);
                if (iMarkedIt) {
                    // Node has been marked
                    find(key, preds, succs);
                    return true;
                } else if (marked[0])
                    return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        boolean[] marked = { false };
        Node pred = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node curr = pred.next[level].getReference();
            while (true) {
                Node succ = curr.next[level].get(marked);
                while (marked[0]) {
                    curr = succ;
                    succ = curr.next[level].get(marked);
                }
                if (curr.key < key) {
                    pred = curr;
                    curr = succ;
                } else
                    break;
            }
        }
        Node curr = pred.next[0].getReference();
        curr.next[0].get(marked);
        return curr.key == key && !marked[0];
    }

    private int randomLevel() {
        int level = 0;
        while (level < MAX_LEVEL && random.nextInt(2) == 0)
            level++;
        return level;
    }
}
