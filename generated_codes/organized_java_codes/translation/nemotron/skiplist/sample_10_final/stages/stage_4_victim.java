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
            head.forward[i].set(tail, false);
            tail.forward[i].set(null, false);
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
            find(key, preds, succs);
            Node succ = succs[0];
            if (succ != tail && succ.key == key) {
                return false;
            }
            int nodeLevel = randomLevel();
            Node newNode = new Node(key, nodeLevel);
            for (int i = 0; i <= nodeLevel; i++) {
                newNode.forward[i].set(succs[i], false);
            }
            boolean spliced = true;
            for (int i = 0; i <= nodeLevel; i++) {
                while (true) {
                    AtomicMarkableReference<Node> predForward = preds[i].forward[i];
                    Node succNode = succs[i];
                    if (predForward.compareAndSet(succNode, newNode, false, false)) {
                        break;
                    } else {
                        find(key, preds, succs);
                        succ = succs[0];
                        if (succ != tail && succ.key == key) {
                            return false;
                        }
                        for (int l = 0; l <= nodeLevel; l++) {
                            newNode.forward[l].set(succs[l], false);
                        }
                    }
                }
            }
            if (spliced) {
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
            Node[] preds = new Node[MAX_LEVEL + 1];
            Node[] succs = new Node[MAX_LEVEL + 1];
            find(key, preds, succs);
            Node target = succs[0];
            if (target == tail || target.key != key) {
                return false;
            }
            // Mark node at each level from top to bottom
            boolean[] marked = new Node[MAX_LEVEL + 1] == null ? new boolean[MAX_LEVEL + 1] : new boolean[MAX_LEVEL + 1];
            boolean allMarked = true;
            for (int i = target.level; i >= 0; i--) {
                while (true) {
                    AtomicMarkableReference<Node> predForward = preds[i].forward[i];
                    if (predForward.attemptMark(target, true)) {
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
                        break;
                    } else {
                        // Help: predecessor changed, restart search
                        find(key, preds, succs);
                        target = succs[0];
                        if (target == tail || target.key != key) {
                            return false;
                        }
                    }
                }
            }
            // Physical removal (optional, helps keep list clean)
            for (int i = 0; i <= target.level; i++) {
                preds[i].forward[i].compareAndSet(target, target.forward[i].getReference(), true, false);
            }
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        Node[] preds = new Node[MAX_LEVEL + 1];
        Node[] succs = new Node[MAX_LEVEL + 1];
        find(key, preds, succs);
        Node succ = succs[0];
        return succ != tail && succ.key == key && !preds[0].forward[0].isMarked();
    }

    private void find(int key, Node[] preds, Node[] succs) {
        Node pred = head;
        for (int level = MAX_LEVEL; level >= 0; level--) {
            Node succ = pred.forward[level].getReference();
            while (true) {
                // Skip marked nodes
                AtomicMarkableReference<Node> ref = pred.forward[level];
                Node curr = ref.getReference();
                boolean marked = ref.isMarked();
                if (marked) {
                    // Help unlink marked node
                    Node next = curr.forward[level].getReference();
                    pred.forward[level].compareAndSet(curr, next, false, false);
                    continue;
                }
                if (curr == tail || curr.key >= key) {
                    succ = curr;
                    break;
                }
                pred = curr;
                succ = pred.forward[level].getReference();
            }
            preds[level] = pred;
            succs[level] = succ;
        }
    }

    private int randomLevel() {
        int lvl = 0;
        while (ThreadLocalRandom.current().nextDouble() < PROBABILITY && lvl < MAX_LEVEL) {
            lvl++;
        }
        return lvl;
    }

    private static class Node {
        int key;
        int level;
        AtomicMarkableReference<Node>[] forward;

        Node(int key, int level) {
            this.key = key;
            this.level = level;
            @SuppressWarnings("unchecked")
            AtomicMarkableReference<Node>[] f = new AtomicMarkableReference[level + 1];
            for (int i = 0; i <= level; i++) {
                f[i] = new AtomicMarkableReference<>(null, false);
            }
            this.forward = f;
        }
    }
}