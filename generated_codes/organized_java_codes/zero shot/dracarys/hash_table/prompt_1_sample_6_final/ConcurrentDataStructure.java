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

    private int index(int key) {
        return Math.abs(key) % buckets.length;
    }

    @Override
    public boolean add(int key) {
        int index = index(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr != null && curr.marked.get()) {
                continue;
            }

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
        int index = index(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr == null) {
                return false;
            }

            if (curr.marked.get()) {
                continue;
            }

            if (curr.key == key) {
                if (curr.marked.compareAndSet(false, true)) {
                    // Node has been marked
                    return true;
                }
            }

            Node next = curr.next;
            if (next != null && next.marked.get()) {
                continue;
            }

            if (bucket.compareAndSet(curr, next)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = index(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            if (curr == null) {
                return false;
            }

            if (curr.marked.get()) {
                continue;
            }

            if (curr.key == key) {
                return true;
            }

            Node next = curr.next;
            if (next != null && next.marked.get()) {
                continue;
            }

            curr = next;
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        AtomicBoolean marked;

        Node() {
            next = new AtomicMarkableReference<>(null, false);
            marked = new AtomicBoolean(false);
        }
    }
}