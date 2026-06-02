package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
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
    private static final int RESIZE_THRESHOLD = 3 / 4;
    private static final int MIN_CAPACITY = 1;

    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicReference<Node>[] table;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        int index = indexFor(hash, table.length);
        Node node = table[index].getReference();
        Node newNode = new Node(key, hash);
        while (node != null) {
            if (node.key == key) {
                return false;
            }
            Node next = node.next.getReference();
            if (next == null) {
                if (node.next.compareAndSet(null, newNode)) {
                    size.incrementAndGet();
                    resizeIfNecessary();
                    return true;
                }
            }
            node = next;
        }
        if (table[index].compareAndSet(null, newNode)) {
            size.incrementAndGet();
            resizeIfNecessary();
            return true;
        }
        return add(key);
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int hash = hash(key);
        int index = indexFor(hash, table.length);
        Node node = table[index].getReference();
        Node prev = null;
        while (node != null) {
            if (node.key == key) {
                if (prev == null) {
                    if (table[index].compareAndSet(node, node.next.getReference())) {
                        size.decrementAndGet();
                        return true;
                    }
                } else {
                    if (prev.next.compareAndSet(node, node.next.getReference())) {
                        size.decrementAndGet();
                        return true;
                    }
                }
            }
            prev = node;
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
        int index = indexFor(hash, table.length);
        Node node = table[index].getReference();
        while (node != null) {
            if (node.key == key && !node.marked.get()) {
                return true;
            }
            node = node.next.getReference();
        }
        return false;
    }

    private void resizeIfNecessary() {
        if (size.get() >= RESIZE_THRESHOLD * table.length) {
            int newCapacity = Math.min(MAX_CAPACITY, table.length * 2);
            AtomicReference<Node>[] newTable = new AtomicReference[newCapacity];
            for (int i = 0; i < newTable.length; i++) {
                newTable[i] = new AtomicReference<>(null);
            }
            for (int i = 0; i < table.length; i++) {
                Node node = table[i].getReference();
                while (node != null) {
                    int newIndex = indexFor(node.hash, newCapacity);
                    Node next = node.next.getReference();
                    if (newTable[newIndex].compareAndSet(null, node)) {
                        node.next.set(next);
                    }
                    node = next;
                }
            }
            table = newTable;
        } else if (size.get() < MIN_CAPACITY && table.length > INITIAL_CAPACITY) {
            int newCapacity = Math.max(INITIAL_CAPACITY, table.length / 2);
            AtomicReference<Node>[] newTable = new AtomicReference[newCapacity];
            for (int i = 0; i < newTable.length; i++) {
                newTable[i] = new AtomicReference<>(null);
            }
            for (int i = 0; i < table.length; i++) {
                Node node = table[i].getReference();
                while (node != null) {
                    int newIndex = indexFor(node.hash, newCapacity);
                    Node next = node.next.getReference();
                    if (newTable[newIndex].compareAndSet(null, node)) {
                        node.next.set(next);
                    }
                    node = next;
                }
            }
            table = newTable;
        }
    }

    private int hash(int key) {
        int h = key;
        h ^= (h >>> 20) ^ (h >>> 12);
        return h ^ (h >>> 7) ^ (h >>> 4);
    }

    private int indexFor(int hash, int length) {
        return hash & (length - 1);
    }

    private static class Node {
        final int key;
        final int hash;
        final AtomicMarkableReference<Node> next;
        final AtomicMarkableReference<Node> marked;

        Node(int key, int hash) {
            this.key = key;
            this.hash = hash;
            this.next = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicMarkableReference<>(null, false);
        }
    }
}