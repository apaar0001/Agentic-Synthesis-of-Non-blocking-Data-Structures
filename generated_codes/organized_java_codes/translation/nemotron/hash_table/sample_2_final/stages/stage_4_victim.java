package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

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


    private static final int INITIAL_CAPACITY = 16;
    private static final float LOAD_FACTOR = 0.75f;

    private final AtomicReference<AtomicMarkableReference<Node>[]> table;
    private final AtomicInteger size;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    public ConcurrentDataStructure() {
        AtomicMarkableReference<Node>[] initTab = new AtomicMarkableReference[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            initTab[i] = new AtomicMarkableReference<>(null, false);
        }
        table.set(initTab);
        size.set(0);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        while (true) {
            AtomicMarkableReference<Node>[] tab = table.get();
            int idx = hash(key, tab.length);
            AtomicMarkableReference<Node> head = tab[idx];
            Node pred = null;
            AtomicMarkableReference<Node> predNext = head;
            Node curr = predNext.getReference();
            boolean currMarked = predNext.isMarked();

            while (true) {
                while (curr != null && currMarked) {
                    Node succ = curr.next.getReference();
                    boolean succMarked = curr.next.isMarked();
                    if (pred == null) {
                        head.compareAndSet(curr, succ, false, false);
                    } else {
                        pred.next.compareAndSet(curr, succ, false, false);
                    }
                    predNext = curr.next;
                    curr = predNext.getReference();
                    currMarked = predNext.isMarked();
                }
                if (curr == null) {
                    break;
                }
                int k = curr.key;
                if (k == key) {
                    return false;
                } else if (k > key) {
                    break;
                } else {
                    pred = curr;
                    predNext = curr.next;
                    curr = predNext.getReference();
                    currMarked = predNext.isMarked();
                }
            }

            Node newNode = new Node(key);
            newNode.next.set(curr, false);
            boolean inserted;
            if (pred == null) {
                inserted = head.compareAndSet(null, newNode, false, false);
            } else {
                inserted = pred.next.compareAndSet(predNext.getReference(), newNode, false, false);
            }
            if (inserted) {
                int s = size.incrementAndGet();
                if (s > (int)(tab.length * LOAD_FACTOR)) {
                    resize(tab);
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
            AtomicMarkableReference<Node>[] tab = table.get();
            int idx = hash(key, tab.length);
            AtomicMarkableReference<Node> head = tab[idx];
            Node pred = null;
            AtomicMarkableReference<Node> predNext = head;
            Node curr = predNext.getReference();
            boolean currMarked = predNext.isMarked();

            while (true) {
                while (curr != null && currMarked) {
                    Node succ = curr.next.getReference();
                    boolean succMarked = curr.next.isMarked();
                    if (pred == null) {
                        head.compareAndSet(curr, succ, false, false);
                    } else {
                        pred.next.compareAndSet(curr, succ, false, false);
                    }
                    predNext = curr.next;
                    curr = predNext.getReference();
                    currMarked = predNext.isMarked();
                }
                if (curr == null) {
                    return false;
                }
                int k = curr.key;
                if (k == key) {
                    boolean marked = predNext.attemptMark(curr, true);
                    if (!marked) {
                        break;
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
                    Node succ = curr.next.getReference();
                    boolean succMarked = curr.next.isMarked();
                    if (pred == null) {
                        head.compareAndSet(curr, succ, false, false);
                    } else {
                        pred.next.compareAndSet(curr, succ, false, false);
                    }
                    size.decrementAndGet();
                    return true;
                } else if (k > key) {
                    return false;
                } else {
                    pred = curr;
                    predNext = curr.next;
                    curr = predNext.getReference();
                    currMarked = predNext.isMarked();
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        AtomicMarkableReference<Node>[] tab = table.get();
        int idx = hash(key, tab.length);
        AtomicMarkableReference<Node> head = tab[idx];
        Node pred = null;
        AtomicMarkableReference<Node> predNext = head;
        Node curr = predNext.getReference();
        boolean currMarked = predNext.isMarked();

        while (true) {
            while (curr != null && currMarked) {
                Node succ = curr.next.getReference();
                boolean succMarked = curr.next.isMarked();
                if (pred == null) {
                    head.compareAndSet(curr, succ, false, false);
                } else {
                    pred.next.compareAndSet(curr, succ, false, false);
                }
                predNext = curr.next;
                curr = predNext.getReference();
                currMarked = predNext.isMarked