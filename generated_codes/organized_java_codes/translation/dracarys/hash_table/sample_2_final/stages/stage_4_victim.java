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

    private final AtomicReference<Node>[] table;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        table = new AtomicReference[INITIAL_CAPACITY];
        size = new AtomicInteger(0);
        for (int i = 0; i < table.length; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return Math.abs(key % table.length);
    }

    private Node newNode(int key) {
        return new Node(key);
    }

    @Override
    public boolean add(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        Node node = ref.getReference();
        while (node != null) {
            if (node.key == key && !node.marked) {
                return false;
            }
            node = node.next.getReference();
        }
        Node newNode = newNode(key);
        while (true) {
            node = ref.getReference();
            newNode.next.set(node);
            if (ref.compareAndSet(node, newNode)) {
                if (size.incrementAndGet() > table.length * LOAD_FACTOR) {
                    resize();
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
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        while (true) {
            Node node = ref.getReference();
            Node prev = null;
            while (node != null) {
                if (node.key == key && !node.marked) {
                    if (node.next.compareAndSet(node.next.getReference(), node.next.getReference())) {
                        node.marked = true;
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
                        if (prev != null) {
                            prev.next.compareAndSet(node, node.next.getReference());
                        } else {
                            ref.compareAndSet(node, node.next.getReference());
                        }
                        size.decrementAndGet();
                        return true;
                    }
                } else if (node.marked) {
                    if (prev != null) {
                        prev.next.compareAndSet(node, node.next.getReference());
                    } else {
                        ref.compareAndSet(node, node.next.getReference());
                    }
                } else {
                    prev = node;
                }
                node = node.next.getReference();
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        if (_lfRetired.get()) {
            return false;
        }
        int index = hash(key);
        AtomicReference<Node> ref = table[index];
        Node node = ref.getReference();
        while (node != null) {
            if (node.key == key && !node.marked) {
                return true;
            }
            node = node.next.getReference();
        }
        return false;
    }

    private void resize() {
        if (table.length >= MAX_CAPACITY) {
            return;
        }
        AtomicReference<Node>[] newTable = new AtomicReference[table.length * 2];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new AtomicReference<>(null);
        }
        for (AtomicReference<Node> ref : table) {
            Node node = ref.getReference();
            while (node != null) {
                Node next = node.next.getReference();
                int newIndex = Math.abs(node.key % newTable.length);
                AtomicReference<Node> newRef = newTable[newIndex];
                while (true) {
                    Node newNext = newRef.getReference();
                    node.next.set(newNext);
                    if (newRef.compareAndSet(newNext, node)) {
                        break;
                    }
                }
                node = next;
            }
        }
        table = newTable;
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        volatile boolean marked;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }
}