package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

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