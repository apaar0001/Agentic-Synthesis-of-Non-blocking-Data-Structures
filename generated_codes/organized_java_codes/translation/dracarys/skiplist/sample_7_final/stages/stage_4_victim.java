package com.example.Sets;

import com.example.utils.SetADT;
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
    private static final int MIN_LEVEL = 1;

    private final AtomicReference<Node> head;

    public ConcurrentDataStructure() {
        head = new AtomicReference<>(new Node(Integer.MIN_VALUE, MAX_LEVEL));
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.get().forward[i].set(tail);
        }
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key == key) {
                return false;
            }
            Node newNode = new Node(key, randomLevel());
            for (int i = 0; i < newNode.level; i++) {
                newNode.forward[i].set(succs[i]);
            }
            if (tryInsert(preds, succ, newNode)) {
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
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key != key) {
                return false;
            }
            if (succ.marked.get()) {
                return false;
            }
            if (tryMark(succ)) {
                if (tryRemove(preds, succ)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            Node[] preds = new Node[MAX_LEVEL];
            Node[] succs = new Node[MAX_LEVEL];
            findPredecessors(key, preds, succs);
            Node succ = succs[0];
            if (succ.key == key && !succ.marked.get()) {
                return true;
            }
            return false;
        }
    }

    private void findPredecessors(int key, Node[] preds, Node[] succs) {
        Node pred = head.get();
        for (int i = pred.level - 1; i >= 0; i--) {
            Node curr = pred.forward[i].getReference();
            while (curr != null && curr.key < key) {
                pred = curr;
                curr = pred.forward[i].getReference();
            }
            preds[i] = pred;
            succs[i] = curr;
        }
    }

    private boolean tryInsert(Node[] preds, Node succ, Node newNode) {
        for (int i = 0; i < newNode.level; i++) {
            if (!preds[i].forward[i].compareAndSet(succ, newNode)) {
                return false;
            }
        }
        return true;
    }

    private boolean tryMark(Node node) {
        return node.marked.compareAndSet(false, true);
    }

    private boolean tryRemove(Node[] preds, Node succ) {
        for (int i = succ.level - 1; i >= 0; i--) {
            if (!preds[i].forward[i].compareAndSet(succ, succ.forward[i].getReference())) {
                return false;
            }
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
        return true;
    }

    private int randomLevel() {
        int level = MIN_LEVEL;
        while (Math.random() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private static class Node {
        final int key;
        final int level;
        final AtomicMarkableReference<Boolean> marked = new AtomicMarkableReference<>(false);
        final AtomicReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            forward = new AtomicReference[level];
            for (int i = 0; i < level; i++) {
                forward[i] = new AtomicReference<>();
            }
        }
    }
}