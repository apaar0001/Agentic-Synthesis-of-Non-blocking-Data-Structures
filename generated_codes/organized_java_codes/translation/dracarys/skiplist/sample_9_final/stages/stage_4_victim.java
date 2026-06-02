package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.Random;

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
    private static final Random random = new Random();
    private final Node head;

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
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
        if (succs[0] != -1 && !marked[0] && succs[0].key == key) {
            return false;
        }
        int level = randomLevel();
        Node node = new Node(key, level);
        if (!tryInsert(node, preds, succs, marked, level)) {
            return add(key);
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
        if (succs[0] == -1 || marked[0] || succs[0].key != key) {
            return false;
        }
        if (!mark(succs, marked, top)) {
            return remove(key);
        }
        if (!tryRemove(succs, marked, top)) {
            return remove(key);
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
        return succs[0] != -1 && !marked[0] && succs[0].key == key;
    }

    private int find(int key, int[] preds, int[] succs, boolean[] marked) {
        int i = MAX_LEVEL - 1;
        Node pred = head;
        Node succ = null;
        while (i >= 0) {
            succ = pred.next[i].getReference();
            while (succ != null && (succ.marked[i] || succ.key > key)) {
                pred = pred.next[i].getReference();
                succ = pred.next[i].getReference();
            }
            preds[i] = pred;
            succs[i] = succ;
            marked[i] = succ != null && succ.marked[i];
            if (succ != null && succ.key == key) {
                return i;
            }
            i--;
        }
        return -1;
    }

    private boolean tryInsert(Node node, int[] preds, int[] succs, boolean[] marked, int level) {
        for (int i = 0; i < level; i++) {
            node.next[i].set(succs[i]);
        }
        for (int i = level - 1; i >= 0; i--) {
            while (true) {
                if (preds[i].next[i].compareAndSet(succs[i], node)) {
                    break;
                }
                find(node.key, preds, succs, marked);
            }
        }
        return true;
    }

    private boolean mark(Node[] succs, boolean[] marked, int top) {
        for (int i = top; i >= 0; i--) {
            while (true) {
                if (succs[i].mark[i].compareAndSet(false, true)) {
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
                    return true;
                }
                if (succs[i].marked[i]) {
                    return false;
                }
            }
        }
        return false;
    }

    private boolean tryRemove(Node[] succs, boolean[] marked, int top) {
        for (int i = top; i >= 0; i--) {
            while (true) {
                if (succs[i].next[i].compareAndSet(succs[i + 1], succs[i])) {
                    break;
                }
                find(succs[i].key, null, succs, marked);
            }
        }
        return true;
    }

    private int randomLevel() {
        int level = 1;
        while (random.nextBoolean() && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private static class Node {
        final int key;
        final int level;
        final AtomicMarkableReference<Node>[] next;
        final AtomicMarkableReference<Boolean>[] mark;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            next = new AtomicMarkableReference[level];
            mark = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
                mark[i] = new AtomicMarkableReference<>(false);
            }
        }
    }
}