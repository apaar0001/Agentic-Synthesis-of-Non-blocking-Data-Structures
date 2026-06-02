package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private Node[] buckets;
    private int capacity;

    public ConcurrentDataStructure() {
        this(DEFAULT_CAPACITY);
    }

    public ConcurrentDataStructure(int capacity) {
        this.capacity = capacity;
        this.buckets = new Node[capacity];

        for (int i = 0; i < capacity; i++) {
            buckets[i] = null;
        }
    }

    private int getIndex(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = new AtomicReference<>(buckets[index]);

        while (true) {
            Node curr = head.get();
            Node next = curr != null ? curr.next.getReference() : null;

            if (next != null && next.isMarked()) {
                helpRemove(index, curr, next);
                continue;
            }

            if (curr != null && curr.key == key) {
                return false;
            }

            Node newNode = new Node(key);
            newNode.next = new AtomicMarkableReference<>(next, false);

            if (head.compareAndSet(curr, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = new AtomicReference<>(buckets[index]);

        while (true) {
            Node curr = head.get();
            Node next = curr != null ? curr.next.getReference() : null;

            if (next != null && next.isMarked()) {
                helpRemove(index, curr, next);
                continue;
            }

            if (curr != null && curr.key == key) {
                if (curr.next.compareAndSet(next, next, false, true)) {
                    // Node has been marked
                    return true;
                }
            }

            if (curr == null || curr.key != key) {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = new AtomicReference<>(buckets[index]);

        while (true) {
            Node curr = head.get();
            Node next = curr != null ? curr.next.getReference() : null;

            if (next != null && next.isMarked()) {
                helpRemove(index, curr, next);
                continue;
            }

            if (curr != null && curr.key == key) {
                if (curr.next.isMarked()) {
                    return false;
                }
                return true;
            }

            if (curr == null) {
                return false;
            }
        }
    }

    private void helpRemove(int index, Node curr, Node next) {
        if (curr.next.compareAndSet(next, next, false, true)) {
            // Node has been marked
            if (buckets[index] == curr) {
                buckets[index] = next;
            }
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }

        boolean isMarked() {
            return next.isMarked();
        }
    }
}