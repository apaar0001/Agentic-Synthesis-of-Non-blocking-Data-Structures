package com.example.Sets;

import com.example.utils.SetADT;
import java.util.Random;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

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
    private static final Random random = new Random();

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node>[] next;
        final int level;

        Node(int key, int level) {
            this.key = key;
            this.next = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
            this.level = level;
        }
    }

    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
    }

    private int randomLevel() {
        int level = 1;
        while (random.nextBoolean() && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private boolean find(int key, AtomicReferenceArray<Node> preds, AtomicReferenceArray<Node> succs) {
        int level = head.level;
        Node pred = head;
        while (level > 0) {
            Node succ = preds.get(level - 1) != null ? preds.get(level - 1) : pred.next[level - 1].getReference();
            while (succ != null && succ.key < key) {
                pred = succ;
                succ = pred.next[level - 1].getReference();
            }
            if (succ != null && succ.key == key) {
                succs.set(level - 1, succ);
            }
            level--;
        }
        return succs.get(0) != null;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicReferenceArray<Node> preds = new AtomicReferenceArray<>(MAX_LEVEL);
        AtomicReferenceArray<Node> succs = new AtomicReferenceArray<>(MAX_LEVEL);
        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }
            int level = randomLevel();
            Node node = new Node(key, level);
            for (int i = 0; i < level; i++) {
                node.next[i].set(succs.get(i), false);
            }
            if (preds.get(level - 1) != null) {
                Node pred = preds.get(level - 1);
                Node succ = succs.get(level - 1);
                if (pred.next[level - 1].compareAndSet(succ, node, false, false)) {
                    return true;
                }
            } else {
                Node pred = head;
                Node succ = succs.get(0);
                if (pred.next[0].compareAndSet(succ, node, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicReferenceArray<Node> preds = new AtomicReferenceArray<>(MAX_LEVEL);
        AtomicReferenceArray<Node> succs = new AtomicReferenceArray<>(MAX_LEVEL);
        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }
            Node node = succs.get(0);
            if (node.next[0].getReference() == null) {
                return false;
            }
            for (int i = 0; i < node.level; i++) {
                node.next[i].set(succs.get(i), true);
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
            for (int i = 0; i < node.level; i++) {
                Node pred = preds.get(i);
                Node succ = succs.get(i);
                if (pred.next[i].compareAndSet(succ, succs.get(i + 1), false, false)) {
                    break;
                }
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicReferenceArray<Node> preds = new AtomicReferenceArray<>(MAX_LEVEL);
        AtomicReferenceArray<Node> succs = new AtomicReferenceArray<>(MAX_LEVEL);
        return find(key, preds, succs);
    }
}