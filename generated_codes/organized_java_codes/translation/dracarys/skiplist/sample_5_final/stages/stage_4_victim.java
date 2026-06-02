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
    private static final double P = 0.5;
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
    private final AtomicReferenceArray<Node> tail;

    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        this.tail = new AtomicReferenceArray<>(new Node[MAX_LEVEL]);
        for (int i = 0; i < MAX_LEVEL; i++) {
            this.tail.set(i, new Node(Integer.MAX_VALUE, 1));
        }
        for (int i = 0; i < MAX_LEVEL; i++) {
            this.head.next[i].set(tail.get(i), false);
        }
    }

    private int randomLevel() {
        int level = 1;
        while (random.nextDouble() < P && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    private Node[] find(int key) {
        Node[] preds = new Node[MAX_LEVEL];
        Node curr = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node succ = curr.next[i].getReference();
                if (succ != null && succ.key <= key) {
                    preds[i] = curr;
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return preds;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int level = randomLevel();
        Node newNode = new Node(key, level);
        while (true) {
            Node[] preds = find(key);
            Node succ = preds[0].next[0].getReference();
            if (succ != null && succ.key == key) {
                return false;
            }
            for (int i = 0; i < level; i++) {
                newNode.next[i].set(tail.get(i), false);
            }
            if (preds[0].next[0].compareAndSet(succ, newNode, false, false)) {
                for (int i = 1; i < level; i++) {
                    while (true) {
                        Node pred = preds[i];
                        Node succ2 = pred.next[i].getReference();
                        if (pred.next[i].compareAndSet(succ2, newNode, false, false)) {
                            break;
                        }
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
        while (true) {
            Node[] preds = find(key);
            Node curr = preds[0].next[0].getReference();
            if (curr == null || curr.key != key) {
                return false;
            }
            if (curr.next[0].attemptMark(null, true)) {
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
                for (int i = 1; i < curr.level; i++) {
                    while (true) {
                        Node pred = preds[i];
                        Node succ = pred.next[i].getReference();
                        if (succ == curr) {
                            pred.next[i].compareAndSet(succ, succ.next[i].getReference(), false, false);
                        } else {
                            break;
                        }
                    }
                }
                preds[0].next[0].compareAndSet(curr, curr.next[0].getReference(), true, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head;
        for (int i = MAX_LEVEL - 1; i >= 0; i--) {
            while (true) {
                Node succ = curr.next[i].getReference();
                if (succ != null && succ.key <= key) {
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        return curr.key == key && !curr.next[0].isMarked();
    }
}