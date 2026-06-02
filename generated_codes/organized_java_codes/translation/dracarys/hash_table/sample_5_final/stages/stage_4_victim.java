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

    private AtomicReference<Node>[] buckets;

    public ConcurrentDataStructure() {
        buckets = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            buckets[i] = new AtomicReference<>();
        }
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int index = hash(key);
        Node node = new Node(key, true);
        while (true) {
            Node current = buckets[index].getReference();
            if (current == null) {
                if (buckets[index].compareAndSet(null, node)) {
                    return true;
                }
            } else {
                Node next = current.next.getReference();
                if (next == null) {
                    if (current.next.compareAndSet(null, node)) {
                        return true;
                    }
                } else {
                    if (next.key == key) {
                        return false;
                    }
                    if (current.next.compareAndSet(next, node)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int index = hash(key);
        while (true) {
            Node current = buckets[index].getReference();
            if (current == null) {
                return false;
            }
            Node next = current.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.key == key) {
                if (next.marked.get()) {
                    return false;
                }
                next.marked.set(true);
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
                if (current.next.compareAndSet(next, next.next.getReference())) {
                    return true;
                }
            } else {
                if (current.next.compareAndSet(next, next.next.getReference())) {
                    return remove(key);
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int index = hash(key);
        Node current = buckets[index].getReference();
        while (current != null) {
            if (current.key == key && !current.marked.get()) {
                return true;
            }
            current = current.next.getReference();
        }
        return false;
    }

    private int hash(int key) {
        return Math.abs(key) % buckets.length;
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicMarkableReference<Boolean> marked;

        Node(int key, boolean marked) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicMarkableReference<>(marked, false);
        }
    }
}