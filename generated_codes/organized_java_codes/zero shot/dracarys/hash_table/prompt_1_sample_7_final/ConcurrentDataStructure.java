package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private AtomicReference<Node>[] buckets;

    public ConcurrentDataStructure() {
        this(DEFAULT_CAPACITY);
    }

    public ConcurrentDataStructure(int capacity) {
        buckets = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new AtomicReference<>(null);
        }
    }

    private int getIndex(int key) {
        return Math.abs(key) % buckets.length;
    }

    @Override
    public boolean add(int key) {
        int index = getIndex(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr != null && curr.isMarked()) {
                helpDelete(bucket, curr);
                continue;
            }

            if (curr != null && curr.key == key) {
                return false;
            }

            Node newNode = new Node(key);
            if (bucket.compareAndSet(curr, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = getIndex(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr != null && curr.isMarked()) {
                helpDelete(bucket, curr);
                continue;
            }

            if (curr == null || curr.key != key) {
                return false;
            }

            Node next = curr.next.get();
            if (curr.next.compareAndSet(next, new Node(key, true))) {
                // Node has been marked
                helpDelete(bucket, curr);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = getIndex(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr != null && curr.isMarked()) {
                helpDelete(bucket, curr);
                continue;
            }

            if (curr == null || curr.key != key) {
                return false;
            }

            return true;
        }
    }

    private void helpDelete(AtomicReference<Node> bucket, Node node) {
        Node next = node.next.get();
        if (bucket.compareAndSet(node, next)) {
            node.next.set(null);
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }

        Node(int key, boolean marked) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, marked);
        }

        boolean isMarked() {
            return next.isMarked();
        }
    }
}