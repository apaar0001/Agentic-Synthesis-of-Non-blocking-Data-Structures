package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicReference<AtomicMarkableReference<Node>> prev;

        public Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
            this.prev = new AtomicReference<>();
        }
    }

    private static final int CAPACITY = 16;
    private final AtomicReference<Node>[] buckets;

    public ConcurrentDataStructure() {
        buckets = new AtomicReference[CAPACITY];
        for (int i = 0; i < CAPACITY; i++) {
            buckets[i] = new AtomicReference<>();
        }
    }

    private int index(int key) {
        return Math.abs(key) % CAPACITY;
    }

    @Override
    public boolean add(int key) {
        int index = index(key);
        Node newNode = new Node(key);
        while (true) {
            Node curr = buckets[index].get();
            Node next = curr == null ? null : curr.next.getReference();
            if (next != null && next.key == key) {
                return false;
            }
            if (curr == null || next == null) {
                if (curr == null) {
                    if (buckets[index].compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    newNode.next.set(next, false);
                    if (curr.next.compareAndSet(next, newNode, false, false)) {
                        return true;
                    }
                }
            } else {
                if (curr.next.compareAndSet(next, newNode, false, false)) {
                    newNode.next.set(next, false);
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = index(key);
        while (true) {
            Node curr = buckets[index].get();
            Node next = curr == null ? null : curr.next.getReference();
            if (next == null || next.key != key) {
                return false;
            }
            if (next.next.compareAndSet(next, next, false, true)) {
                // Node has been marked
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = index(key);
        while (true) {
            Node curr = buckets[index].get();
            Node next = curr == null ? null : curr.next.getReference();
            if (next == null) {
                return false;
            }
            if (next.key == key) {
                if (next.next.isMarked()) {
                    if (curr.next.compareAndSet(next, next.next.getReference(), false, false)) {
                        continue;
                    }
                }
                return !next.next.isMarked();
            }
            if (next.next.isMarked()) {
                if (curr.next.compareAndSet(next, next.next.getReference(), false, false)) {
                    continue;
                }
            } else {
                curr = next;
            }
        }
    }
}