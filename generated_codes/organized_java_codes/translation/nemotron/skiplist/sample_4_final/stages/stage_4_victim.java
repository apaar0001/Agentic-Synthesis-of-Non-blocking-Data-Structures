package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.ThreadLocalRandom;

public class ConcurrentDataStructure implements SetADT {
    // Lock-freedom test helpers (auto-injected)
    private static final java.util.concurrent.atomic.AtomicBoolean _lfVictimChosen = new java.util.concurrent.atomic.AtomicBoolean(false);
    private static final ThreadLocal<Integer> _lfOpCount = ThreadLocal.withInitial(() -> 0);
    private static final ThreadLocal<Boolean> _lfRetired =
            ThreadLocal.withInitial(() -> false);

    /**
     * Decide if the *current* thread should become the victim.
     *
     * Each call increments a per-thread operation counter. Once a thread
     * has executed more than 100 operations and no victim has been chosen,
     * it atomically claims the victim role and will then stall.
     */
    private static boolean _lfShouldStall() {
        int c = _lfOpCount.get() + 1;
        _lfOpCount.set(c);
        if (c > 100 && !_lfVictimChosen.get() && _lfVictimChosen.compareAndSet(false, true)) {
            return true;
        }
        return false;
    }


    private static final int MAX_LEVEL = 16;
    private static final double PROBABILITY = 0.5;

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
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            Node pred = head;
            boolean valid = true;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                Node curr = pred.forward[level].getReference();
                while (true) {
                    if (pred.forward[level].isMarked()) {
                        Node next = curr.forward[level].getReference();
                        if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                            curr = next;
                            continue;
                        } else {
                            valid = false;
                            break;
                        }
                    }
                    if (curr.key >= key) {
                        break;
                    }
                    pred = curr;
                    curr = pred.forward[level].getReference();
                }
                if (!valid) break;
                preds[level] = pred;
                succs[level] = curr;
            }
            if (!valid) continue;
            Node first = succs[0];
            if (first.key == key && !first.isMarked()) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            for (int i = 0; i <= newLevel; i++) {
                newNode.forward[i] = new AtomicMarkableReference<>(succs[i], false);
            }
            boolean spliced = true;
            for (int i = 0; i <= newLevel; i++) {
                while (true) {
                    Node predNode = preds[i];
                    Node succNode = succs[i];
                    if (predNode.forward[i].compareAndSet(succNode, newNode, false, false)) {
                        break;
                    } else {
                        if (predNode.forward[i].isMarked()) {
                            Node marked = predNode.forward[i].getReference();
                            Node next = marked.forward[i].getReference();
                            if (predNode.forward[i].compareAndSet(marked, next, false, false)) {
                                continue;
                            }
                        }
                        spliced = false;
                        break;
                    }
                }
                if (!spliced) break;
            }
            if (spliced) return true;
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            Node pred = head;
            boolean valid = true;
            for (int level = MAX_LEVEL; level >= 0; level--) {
                Node curr = pred.forward[level].getReference();
                while (true) {
                    if (pred.forward[level].isMarked()) {
                        Node next = curr.forward[level].getReference();
                        if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                            curr = next;
                            continue;
                        } else {
                            valid = false;
                            break;
                        }
                    }
                    if (curr.key >= key) {
                        break;
                    }
                    pred = curr;
                    curr = pred.forward[level].getReference();
                }
                if (!valid) break;
                preds[level] = pred;
                succs[level] = curr;
            }
            if (!valid) continue;
            Node nodeToDelete = succs[0];
            if (nodeToDelete.key != key || nodeToDelete.isMarked()) {
                return false;
            }
            boolean marked = nodeToDelete.forward[0].attemptMark(nodeToDelete.forward[0].getReference(), true);
            if (!marked) {
                return false;
            }
            // Node has been marked
            // Lock-freedom victim stall injection (auto-injected)
            if (_lfShouldStall()) {
                System.err.println("LOG: Victim thread stalling inside remove()");
                try {
                    Thread.sleep(10_000);
                } catch (InterruptedException ignored) {
                }
                System.err.println("LOG: Victim resumed and retiring");
                _lfRetired.set(true);
                return false;
            }
            boolean removed = true;
            for (int level = 0; level <= nodeToDelete.level; level++) {
                while (true) {
                    Node predNode = preds[level];
                    Node succNode = nodeToDelete.forward[level].getReference();
                    if (predNode.forward[level].compareAndSet(nodeToDelete, succNode, false, false)) {
                        break;
                    } else {
                        if (predNode.forward[level].isMarked()) {
                            Node markedNode = predNode.forward[level].getReference();
                            Node next = markedNode.forward[level].getReference();
                            if (predNode.forward[level].compareAndSet(markedNode, next, false, false)) {
                                continue;
                            }
                        }
                        removed = false;
                        break;
                    }
                }
                if (!removed) break;
            }
            if (removed) return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node pred = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node curr = pred.forward[level].getReference();
            while (true) {
                if (pred.forward[level].isMarked()) {
                    Node next = curr.forward[level].getReference();
                    if (pred.forward[level].compareAndSet(curr, next, false, false)) {
                        curr = next;
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = pred.forward[level].getReference();
            }
            if (level == 0) {
                return (curr.key == key) && !curr.isMarked();
            }
        }
        return false;
    }

    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && level < MAX_LEVEL - 1) {
            level++;
        }
        return level;
    }

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.forward = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                this.forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }

        boolean isMarked() {
            return forward[0].isMarked();
        }
    }
}