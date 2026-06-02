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

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] next;

        @SuppressWarnings("unchecked")
        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.next = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    private final Node head;
    private final Node tail;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i <= MAX_LEVEL; i++) {
            head.next[i].set(tail, false);
            tail.next[i].set(null, false);
        }
    }

    private int randomLevel() {
        int level = 0;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private void find(int key, Node[] pred, Node[] succ) {
        int level = MAX_LEVEL;
        Node prev = head;
        Node curr;
        boolean marked;
        while (level >= 0) {
            curr = prev.next[level].getReference();
            marked = prev.next[level].isMarked();
            while (marked || (curr != tail && curr.key < key)) {
                if (marked) {
                    Node succNext = curr.next[level].getReference();
                    prev.next[level].compareAndSet(curr, true, succNext, false);
                    curr = prev.next[level].getReference();
                    marked = prev.next[level].isMarked();
                } else {
                    prev = curr;
                    curr = prev.next[level].getReference();
                    marked = prev.next[level].isMarked();
                }
            }
            pred[level] = prev;
            succ[level] = curr;
            level--;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] pred = new Node[MAX_LEVEL + 1];
        Node[] succ = new Node[MAX_LEVEL + 1];
        find(key, pred, succ);
        Node first = succ[0];
        return first != null && first.key == key && !first.next[0].isMarked();
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            find(key, pred, succ);
            Node first = succ[0];
            if (first != null && first.key == key) {
                return false;
            }
            int newLevel = randomLevel();
            Node newNode = new Node(key, newLevel);
            for (int i = 0; i <= newLevel; i++) {
                newNode.next[i].set(succ[i], false);
            }
            boolean success = true;
            for (int i = 0; i <= newLevel; i++) {
                AtomicMarkableReference<Node> predNext = pred[i].next[i];
                while (true) {
                    Node ref = predNext.getReference();
                    boolean marked = predNext.isMarked();
                    if (marked) {
                        success = false;
                        break;
                    }
                    if (predNext.compareAndSet(ref, marked, newNode, false)) {
                        break;
                    }
                }
                if (!success) break;
            }
            if (success) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] pred = new Node[MAX_LEVEL + 1];
            Node[] succ = new Node[MAX_LEVEL + 1];
            find(key, pred, succ);
            Node nodeToRemove = succ[0];
            if (nodeToRemove == null || nodeToRemove.key != key) {
                return false;
            }
            AtomicMarkableReference<Node> next0 = nodeToRemove.next[0];
            boolean marked = false;
            while (!marked) {
                Node ref = next0.getReference();
                boolean isMarked = next0.isMarked();
                if (isMarked) {
                    marked = true;
                    break;
                }
                if (next0.compareAndSet(ref, isMarked, ref, true)) {
                    marked = true;
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
                }
            }
            boolean removed = true;
            for (int i = 0; i <= nodeToRemove.level; i++) {
                AtomicMarkableReference<Node> predNext = pred[i].next[i];
                Node expected = nodeToRemove;
                boolean expMarked = false;
                Node newRef = nodeToRemove.next[i].getReference();
                if (!predNext.compareAndSet(expected, expMarked, newRef, false)) {
                    removed = false;
                    break;
                }
            }
            if (removed) {
                return true;
            }
        }
    }
}