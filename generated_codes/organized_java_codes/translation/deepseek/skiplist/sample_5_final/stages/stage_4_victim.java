package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
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

    private static final int MAX_LEVEL = 32;
    private final Node head;
    private final AtomicInteger size;

    private static final class Node {
        final int key;
        final AtomicMarkableReference<Node>[] forward;
        final int topLevel;

        Node(int key, int height) {
            this.key = key;
            this.topLevel = height;
            this.forward = new AtomicMarkableReference[height];
            for (int i = 0; i < height; i++) {
                forward[i] = new AtomicMarkableReference<>(null, false);
            }
        }
    }

    public ConcurrentDataStructure() {
        head = new Node(Integer.MIN_VALUE, MAX_LEVEL);
        Node tail = new Node(Integer.MAX_VALUE, MAX_LEVEL);
        for (int i = 0; i < MAX_LEVEL; i++) {
            head.forward[i].set(tail, false);
        }
        size = new AtomicInteger(0);
    }

    private int randomLevel() {
        int level = 1;
        while (ThreadLocalRandom.current().nextDouble() < 0.5 && level < MAX_LEVEL) {
            level++;
        }
        return level;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int topLevel = randomLevel();
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];

        while (true) {
            if (find(key, preds, succs)) {
                return false;
            }

            Node newNode = new Node(key, topLevel);
            for (int level = 0; level < topLevel; level++) {
                Node succ = succs[level];
                newNode.forward[level].set(succ, false);
            }

            Node pred = preds[0];
            Node succ = succs[0];
            if (!pred.forward[0].compareAndSet(succ, newNode, false, false)) {
                continue;
            }

            for (int level = 1; level < topLevel; level++) {
                while (true) {
                    pred = preds[level];
                    succ = succs[level];
                    if (pred.forward[level].compareAndSet(succ, newNode, false, false)) {
                        break;
                    }
                    find(key, preds, succs);
                }
            }
            size.incrementAndGet();
            return true;
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL];
        Node[] succs = new Node[MAX_LEVEL];
        Node victim;

        while (true) {
            if (!find(key, preds, succs)) {
                return false;
            }

            victim = succs[0];
            if (victim.key != key) {
                return false;
            }

            for (int level = victim.topLevel - 1; level >= 1; level--) {
                boolean[] marked = {false};
                Node succ = victim.forward[level].get(marked);
                while (!marked[0]) {
                    victim.forward[level].compareAndSet(succ, succ, false, true);
                    succ = victim.forward[level].get(marked);
                }
            }

            boolean[] marked = {false};
            Node succ = victim.forward[0].get(marked);
            while (true) {
                boolean iMarkedIt = victim.forward[0].compareAndSet(succ, succ, false, true);
                succ = succs[0].forward[0].get(marked);
                if (iMarkedIt) {
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
                    find(key, preds, succs);
                    size.decrementAndGet();
                    return true;
                } else if (marked[0]) {
                    return false;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node curr = head;
        for (int level = MAX_LEVEL - 1; level >= 0; level--) {
            while (true) {
                boolean[] marked = {false};
                Node succ = curr.forward[level].get(marked);
                while (marked[0]) {
                    succ = succ.forward[level].get(marked);
                }
                if (succ.key < key) {
                    curr = succ;
                } else {
                    break;
                }
            }
        }
        boolean[] marked = {false};
        Node succ = curr.forward[0].get(marked);
        return succ.key == key && !marked[0];
    }

    private boolean find(int key, Node[] preds, Node[] succs) {
        boolean[] marked = {false};
        boolean snip;
        Node pred, curr, succ;

        retry:
        while (true) {
            pred = head;
            for (int level = MAX_LEVEL - 1; level >= 0; level--) {
                curr = pred.forward[level].getReference();
                while (true) {
                    succ = curr.forward[level].get(marked);
                    while (marked[0]) {
                        snip = pred.forward[level].compareAndSet(curr, succ, false, false);
                        if (!snip) {
                            continue retry;
                        }
                        curr = pred.forward[level].getReference();
                        succ = curr.forward[level].get(marked);
                    }
                    if (curr.key < key) {
                        pred = curr;
                        curr = succ;
                    } else {
                        break;
                    }
                }
                preds[level] = pred;
                succs[level] = curr;
            }
            return curr.key == key;
        }
    }
}