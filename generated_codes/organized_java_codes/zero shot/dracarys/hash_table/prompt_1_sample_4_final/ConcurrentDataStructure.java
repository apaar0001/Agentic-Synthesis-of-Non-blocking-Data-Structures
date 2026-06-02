package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private AtomicReference<Node>[] buckets;
    private int capacity;

    public ConcurrentDataStructure() {
        this(DEFAULT_CAPACITY);
    }

    public ConcurrentDataStructure(int capacity) {
        this.capacity = capacity;
        buckets = new AtomicReference[capacity];

        for (int i = 0; i < capacity; i++) {
            buckets[i] = new AtomicReference<>(null);
        }
    }

    private int getIndex(int key) {
        return Math.abs(key) % capacity;
    }

    private AtomicMarkableReference<Node> getMarkableReference(Node node) {
        return new AtomicMarkableReference<>(node, false);
    }

    @Override
    public boolean add(int key) {
        int index = getIndex(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            Node next = curr != null ? curr.next : null;

            if (curr != null && curr.key == key) {
                return false;
            }

            Node newNode = new Node();
            newNode.key = key;
            newNode.next = curr;

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
            Node next = curr != null ? curr.next : null;

            if (curr == null || curr.key > key) {
                return false;
            }

            if (curr.key == key) {
                AtomicMarkableReference<Node> markedRef = getMarkableReference(curr);
                if (markedRef.isMarked()) {
                    return false;
                }

                if (markedRef.attemptMark(curr, true)) {
                    // Node has been marked
                    return true;
                }
            }

            if (curr != null && curr.next != null && curr.next.key == key) {
                AtomicMarkableReference<Node> nextMarkedRef = getMarkableReference(curr.next);
                if (nextMarkedRef.attemptMark(curr.next, true)) {
                    // Node has been marked
                    curr.next = curr.next.next;
                }
            }

            bucket.compareAndSet(curr, next);
        }
    }

    @Override
    public boolean contains(int key) {
        int index = getIndex(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            Node next = curr != null ? curr.next : null;

            if (curr == null || curr.key > key) {
                return false;
            }

            if (curr.key == key) {
                AtomicMarkableReference<Node> markedRef = getMarkableReference(curr);
                if (markedRef.isMarked()) {
                    return false;
                } else {
                    return true;
                }
            }

            bucket.compareAndSet(curr, next);
        }
    }

    private static class Node {
        int key;
        Node next;
    }
}