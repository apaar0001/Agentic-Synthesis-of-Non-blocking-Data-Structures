package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static final int INITIAL_CAPACITY = 16;
    private static final int MAX_CAPACITY = 1 << 30;
    private static final float LOAD_FACTOR = 0.75f;

    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger capacity = new AtomicInteger(INITIAL_CAPACITY);
    private final Node[] table;

    public ConcurrentDataStructure() {
        table = new Node[INITIAL_CAPACITY];
        for (int i = 0; i < table.length; i++) {
            table[i] = new Node(Integer.MIN_VALUE, false);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = hash(key);
        int index = indexFor(hash, capacity.get());
        Node node = table[index];
        Node newNode = new Node(key, false);
        while (true) {
            Node next = node.next.getReference();
            if (next != null && next.key == key) {
                return false;
            }
            if (node.next.compareAndSet(next, newNode)) {
                size.incrementAndGet();
                if (size.get() > LOAD_FACTOR * capacity.get()) {
                    resize();
                }
                return true;
            }
            node = table[index];
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = hash(key);
        int index = indexFor(hash, capacity.get());
        Node node = table[index];
        while (true) {
            Node next = node.next.getReference();
            if (next == null || next.key > key) {
                return false;
            }
            if (next.key == key) {
                if (next.marked.get()) {
                    return false;
                }
                AtomicMarkableReference<Node> nextRef = next.next;
                if (nextRef.compareAndSet(null, new Node(Integer.MIN_VALUE, true))) {
                    // Node has been marked
                    if (node.next.compareAndSet(next, nextRef.getReference())) {
                        size.decrementAndGet();
                        return true;
                    }
                }
            }
            node = table[index];
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = hash(key);
        int index = indexFor(hash, capacity.get());
        Node node = table[index];
        while (true) {
            Node next = node.next.getReference();
            if (next == null || next.key > key) {
                return false;
            }
            if (next.key == key && !next.marked.get()) {
                return true;
            }
            node = table[index];
        }
    }

    private void resize() {
        int newCapacity = capacity.get() << 1;
        if (newCapacity > MAX_CAPACITY) {
            return;
        }
        Node[] newTable = new Node[newCapacity];
        for (int i = 0; i < newTable.length; i++) {
            newTable[i] = new Node(Integer.MIN_VALUE, false);
        }
        for (Node node : table) {
            Node next = node.next.getReference();
            while (next != null) {
                int index = indexFor(next.key, newCapacity);
                Node newNext = newTable[index].next.getReference();
                if (newNext == null) {
                    newTable[index].next.set(next);
                } else {
                    next.next.set(newNext);
                }
                next = next.next.getReference();
            }
        }
        table = newTable;
        capacity.set(newCapacity);
    }

    private int hash(int key) {
        int h = key;
        h ^= (h >>> 20) ^ (h >>> 12);
        return h ^ (h >>> 7) ^ (h >>> 4);
    }

    private int indexFor(int hash, int capacity) {
        return (hash & (capacity - 1));
    }

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;
        final AtomicMarkableReference<Boolean> marked;

        Node(int key, boolean marked) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
            this.marked = new AtomicMarkableReference<>(marked);
        }
    }
}