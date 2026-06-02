package com.example.Sets;

import com.example.utils.SetADT;
import java.util.Random;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;

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
    private static final int MIN_LEVEL = 1;
    private static final int P = 2; // probability of a node having a higher level
    private static final int NULL_KEY = Integer.MIN_VALUE;

    private Node head;
    private AtomicInteger level;

    public ConcurrentDataStructure() {
        this.level = new AtomicInteger(MIN_LEVEL);
        this.head = new Node(NULL_KEY, MAX_LEVEL);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int[] preds = new int[MAX_LEVEL];
        int[] succs = new int[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        int top = find(key, preds, succs, marked);
        if (succs[0] != NULL_KEY && succs[0] == key) {
            return false;
        }
        int newLevel = randomLevel();
        if (newLevel > level.get()) {
            while (level.get() < newLevel) {
                if (level.compareAndSet(level.get(), newLevel)) {
                    break;
                }
            }
        }
        Node newNode = new Node(key, newLevel);
        for (int i = 0; i < newLevel; i++) {
            newNode.next[i].set(succs[i]);
            marked[i] = false;
        }
        for (int i = 0; i < newLevel; i++) {
            while (true) {
                if (preds[i].next[i].getReference() == succs[i] && !marked[i]) {
                    if (preds[i].next[i].compareAndSet(succs[i], newNode)) {
                        break;
                    }
                } else {
                    find(key, preds, succs, marked);
                    if (succs[0] != NULL_KEY && succs[0] == key) {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int[] preds = new int[MAX_LEVEL];
        int[] succs = new int[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        int top = find(key, preds, succs, marked);
        if (succs[0] == NULL_KEY || succs[0] != key) {
            return false;
        }
        for (int i = top - 1; i >= 0; i--) {
            boolean[] markedRef = new boolean[1];
            Node succ = succs[i];
            while (!succ.next[i].compareAndSet(succs[i], succs[i], false, markedRef)) {
                if (markedRef[0]) {
                    find(key, preds, succs, marked);
                    if (succs[0] == NULL_KEY || succs[0] != key) {
                        return false;
                    }
                    succ = succs[i];
                }
            }
            marked[i] = true;
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
        for (int i = top - 1; i >= 0; i--) {
            while (true) {
                if (preds[i].next[i].getReference() == succs[i] && marked[i]) {
                    preds[i].next[i].compareAndSet(succs[i], succs[i].next[i].getReference());
                    break;
                } else {
                    find(key, preds, succs, marked);
                    if (succs[0] == NULL_KEY || succs[0] != key) {
                        return true;
                    }
                }
            }
        }
        return true;
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int[] preds = new int[MAX_LEVEL];
        int[] succs = new int[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        int top = find(key, preds, succs, marked);
        return succs[0] != NULL_KEY && succs[0] == key;
    }

    private int find(int key, int[] preds, int[] succs, boolean[] marked) {
        int top = -1;
        Node pred = head;
        for (int i = level.get() - 1; i >= 0; i--) {
            Node succ = pred.next[i].getReference();
            while (succ != null && (succ.key < key || (succ.key == key && marked[i]))) {
                pred = succ;
                succ = pred.next[i].getReference();
            }
            preds[i] = pred;
            succs[i] = succ;
            marked[i] = succ != null && succ.key == key;
            top = i;
        }
        return top;
    }

    private int randomLevel() {
        int level = MIN_LEVEL;
        Random random = new Random();
        while (random.nextInt(100) < P && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] next;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            this.next = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                this.next[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }
}