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
    private static final int MAX_CAPACITY = 1 << 30;
    private static final float LOAD_FACTOR = 0.75f;

    private AtomicReference<Node>[] table;
    private AtomicInteger size;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        size = new AtomicInteger(0);
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        int h = key ^ (key >>> 16);
        return h & (table.length - 1);
    }

    private Node newNode(int key, Node next) {
        return new Node(key, next);
    }

    private boolean resize() {
        int newCapacity = table.length << 1;
        if (newCapacity > MAX_CAPACITY) return false;

        AtomicReference<Node>[] newTable = new AtomicReference[newCapacity];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new AtomicReference<>(null);
        }

        for (int i = 0; i < table.length; i++) {
            Node node = table[i].get();
            while (node != null) {
                Node next = node.next.getReference();
                int newHash = node.key ^ (node.key >>> 16);
                newHash &= (newTable.length - 1);
                Node newNext = newTable[newHash].get();
                while (newNext != null && newNext.key < node.key) {
                    newNext = newNext.next.getReference();
                }
                if (newNext == null) {
                    if (newTable[newHash].compareAndSet(null, newNode(node.key, null))) {
                        node.next.set(newNext);
                        node = next;
                    } else {
                        newNext = newTable[newHash].get();
                    }
                } else {
                    Node newPrev = newNext;
                    while (newNext != null && newNext.key < node.key) {
                        newPrev = newNext;
                        newNext = newNext.next.getReference();
                    }
                    if (newNext == null) {
                        if (newPrev.next.compareAndSet(null, newNode(node.key, null))) {
                            node.next.set(newNext);
                            node = next;
                        } else {
                            newNext = newPrev.next.getReference();
                        }
                    } else {
                        Node newNextNode = newNode(node.key, newNext);
                        if (newPrev.next.compareAndSet(newNext, newNextNode)) {
                            node.next.set(newNext);
                            node = next;
                        }
                    }
                }
            }
        }

        table = newTable;
        return true;
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        Node node = table[hash].get();
        while (node != null) {
            if (node.key == key) return false;
            node = node.next.getReference();
        }

        Node newNode = newNode(key, null);
        while (true) {
            node = table[hash].get();
            Node next = node.next.getReference();
            if (node == null) {
                if (table[hash].compareAndSet(null, newNode)) {
                    size.incrementAndGet();
                    if (size.get() > table.length * LOAD_FACTOR) resize();
                    return true;
                }
            } else if (next == null) {
                if (node.next.compareAndSet(null, newNode)) {
                    size.incrementAndGet();
                    if (size.get() > table.length * LOAD_FACTOR) resize();
                    return true;
                }
            } else {
                Node newNext = next.next.getReference();
                if (newNext == null) {
                    if (next.next.compareAndSet(null, newNode)) {
                        size.incrementAndGet();
                        if (size.get() > table.length * LOAD_FACTOR) resize();
                        return true;
                    }
                } else {
                    Node newPrev = next;
                    while (newNext != null && newNext.key < key) {
                        newPrev = newNext;
                        newNext = newNext.next.getReference();
                    }
                    if (newNext == null) {
                        if (newPrev.next.compareAndSet(null, newNode)) {
                            size.incrementAndGet();
                            if (size.get() > table.length * LOAD_FACTOR) resize();
                            return true;
                        }
                    } else {
                        Node newNextNode = newNode(key, newNext);
                        if (newPrev.next.compareAndSet(newNext, newNextNode)) {
                            size.incrementAndGet();
                            if (size.get() > table.length * LOAD_FACTOR) resize();
                            return true;
                        }
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
        int hash = hash(key);
        Node node = table[hash].get();
        while (node != null) {
            if (node.key == key) {
                AtomicMarkableReference<Node> next = node.next;
                while (next.isMarked()) {
                    next = next.getReference().next;
                }
                if (next.getReference() == null) return false;
                if (node.next.compareAndSet(next.getReference(), next.getReference().next)) {
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
                    size.decrementAndGet();
                    return true;
                }
            }
            node = node.next.getReference();
        }
        return false;
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        Node node = table[hash].get();
        while (node != null) {
            if (node.key == key) return true;
            node = node.next.getReference();
        }
        return false;
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }
}