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
        if (_lfRetired.get()) {
            return false;
        }
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
        if (_lfRetired.get()) {
            return false;
        }
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