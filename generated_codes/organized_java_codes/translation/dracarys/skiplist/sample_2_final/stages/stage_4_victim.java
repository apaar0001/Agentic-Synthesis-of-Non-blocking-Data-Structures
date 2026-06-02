package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicInteger;

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
    private static final int P = 4; // 1 / (1 << P) probability of level increase

    private AtomicReference<Node> head;
    private AtomicInteger level;

    public ConcurrentDataStructure() {
        Node dummy = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        head = new AtomicReference<>(dummy);
        level = new AtomicInteger(MIN_LEVEL);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        boolean[] valid = new boolean[MAX_LEVEL];
        Node node = null;
        int top = -1;

        while (true) {
            top = find(key, preds, succs, marked, valid);
            if (succs[top].getReference() != null && succs[top].getReference().key == key) {
                return false;
            }
            node = new Node(key, randomLevel());
            node.next[0].set(succs[0].getReference());
            for (int i = 1; i < node.level; i++) {
                node.next[i].set(succs[i].getReference());
            }
            if (cas(preds[0], succs[0], node)) {
                for (int i = 1; i < node.level; i++) {
                    if (cas(preds[i], succs[i], node)) {
                        // Node has been added
                    } else {
                        // Node has not been added, try again
                        return add(key);
                    }
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        boolean[] valid = new boolean[MAX_LEVEL];
        int top = -1;

        while (true) {
            top = find(key, preds, succs, marked, valid);
            if (succs[top].getReference() == null || succs[top].getReference().key != key) {
                return false;
            }
            Node node = succs[top].getReference();
            if (node.marked[0].get()) {
                return false;
            }
            if (node.marked[0].compareAndSet(false, true)) {
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
                    if (cas(preds[i], succs[i], node)) {
                        // Node has been removed
                    } else {
                        // Node has not been removed, try again
                        return remove(key);
                    }
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        boolean[] marked = new boolean[MAX_LEVEL];
        boolean[] valid = new boolean[MAX_LEVEL];
        int top = -1;

        while (true) {
            top = find(key, preds, succs, marked, valid);
            if (succs[top].getReference() != null && succs[top].getReference().key == key && !succs[top].getReference().marked[0].get()) {
                return true;
            }
            return false;
        }
    }

    private int find(int key, Node[] preds, Node[] succs, boolean[] marked, boolean[] valid) {
        int i = level.get() - 1;
        Node curr = head.get();
        while (true) {
            while (true) {
                Node succ = curr.next[i].getReference();
                if (succ != null && succ.key < key) {
                    curr = succ;
                } else {
                    preds[i] = curr;
                    succs[i] = curr.next[i];
                    marked[i] = succ != null && succ.marked[0].get();
                    valid[i] = true;
                    break;
                }
            }
            if (i == 0) {
                return 0;
            }
            if (valid[i - 1]) {
                i--;
            } else {
                curr = head.get();
            }
        }
    }

    private boolean cas(Node pred, AtomicMarkableReference<Node> succ, Node node) {
        return succ.compareAndSet(pred, node, false, false);
    }

    private int randomLevel() {
        int level = 1;
        while (Math.random() < 1.0 / (1 << P) && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] next;
        AtomicMarkableReference<Boolean>[] marked;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            next = new AtomicMarkableReference[level];
            marked = new AtomicMarkableReference[level];
            for (int i = 0; i < level; i++) {
                next[i] = new AtomicMarkableReference<>(null, false);
                marked[i] = new AtomicMarkableReference<>(false, false);
            }
        }
    }
}