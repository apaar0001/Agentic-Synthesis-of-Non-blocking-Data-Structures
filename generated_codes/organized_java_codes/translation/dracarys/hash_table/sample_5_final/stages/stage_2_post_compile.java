package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

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