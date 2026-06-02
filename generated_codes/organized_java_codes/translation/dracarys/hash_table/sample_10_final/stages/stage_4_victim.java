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
    private static final int RESIZE_THRESHOLD = 16;

    private static class Node {
        final int key;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private static class Bucket {
        AtomicReference<Node> head;

        Bucket() {
            this.head = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Bucket[]> table;

    public ConcurrentDataStructure() {
        table = new AtomicReference<>(new Bucket[INITIAL_CAPACITY]);
    }

    private int hash(int key) {
        return key & (table.get().length - 1);
    }

    private boolean add(int key) {
        int hash = hash(key);
        Bucket[] table = this.table.get();
        Bucket bucket = table[hash];
        Node head = bucket.head.get();
        Node newNode = new Node(key);

        while (true) {
            Node next = head.next.getReference();
            if (next != null && next.key == key) {
                return false;
            }
            if (bucket.head.compareAndSet(head, newNode)) {
                newNode.next.set(next);
                return true;
            }
            head = bucket.head.get();
        }
    }

    private boolean remove(int key) {
        int hash = hash(key);
        Bucket[] table = this.table.get();
        Bucket bucket = table[hash];
        Node head = bucket.head.get();

        while (true) {
            Node next = head.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.key == key) {
                if (next.next.getReference() == null) {
                    if (bucket.head.compareAndSet(head, next)) {
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
                } else {
                    if (next.next.getReference().next.compareAndSet(next, next.next.getReference())) {
                        // Node has been marked
                        return true;
                    }
                }
            }
            head = bucket.head.get();
        }
    }

    private boolean contains(int key) {
        int hash = hash(key);
        Bucket[] table = this.table.get();
        Bucket bucket = table[hash];
        Node head = bucket.head.get();

        while (true) {
            Node next = head.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.key == key) {
                return true;
            }
            head = bucket.head.get();
        }
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        if (add(key)) {
            return true;
        }
        if (resize()) {
            return add(key);
        }
        return false;
    }

    @Override
    public boolean remove(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return remove(key);
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        return contains(key);
    }

    private boolean resize() {
        Bucket[] oldTable = table.get();
        int oldCapacity = oldTable.length;
        if (oldCapacity >= MAX_CAPACITY) {
            return false;
        }
        Bucket[] newTable = new Bucket[oldCapacity << 1];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new Bucket();
        }
        for (int i = 0; i < oldCapacity; i++) {
            Bucket oldBucket = oldTable[i];
            Node oldHead = oldBucket.head.get();
            while (oldHead != null) {
                Node next = oldHead.next.getReference();
                int newHash = oldHead.key & (newTable.length - 1);
                Bucket newBucket = newTable[newHash];
                Node newHead = newBucket.head.get();
                while (true) {
                    Node newNext = newHead.next.getReference();
                    if (newNext == null) {
                        if (newBucket.head.compareAndSet(newHead, oldHead)) {
                            break;
                        }
                    } else {
                        if (newNext.next.getReference().next.compareAndSet(newNext, oldHead)) {
                            break;
                        }
                    }
                    newHead = newBucket.head.get();
                }
                oldHead = next;
            }
        }
        return table.compareAndSet(oldTable, newTable);
    }
}