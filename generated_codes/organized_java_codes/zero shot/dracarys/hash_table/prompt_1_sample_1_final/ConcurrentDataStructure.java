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
        this.buckets = new AtomicReference[capacity];

        for (int i = 0; i < capacity; i++) {
            this.buckets[i] = new AtomicReference<>(null);
        }
    }

    private int getIndex(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = buckets[index];

        while (true) {
            Node curr = head.get();
            Node prev = null;
            Node next = null;

            while (curr != null) {
                next = curr.next.getReference();
                if (curr.key == key) {
                    return false;
                }
                prev = curr;
                curr = next;
            }

            Node newNode = new Node();
            newNode.key = key;
            newNode.next = new AtomicMarkableReference<>(curr, false);

            if (prev == null) {
                if (head.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (prev.next.compareAndSet(curr, newNode)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = buckets[index];

        while (true) {
            Node curr = head.get();
            Node prev = null;

            while (curr != null) {
                AtomicMarkableReference<Node> next = curr.next;
                Node nextNode = next.getReference();

                if (curr.key == key) {
                    if (next.isMarked()) {
                        return false;
                    }

                    if (next.attemptMark(nextNode, true)) {
                        // Node has been marked
                        return true;
                    }
                }

                prev = curr;
                curr = nextNode;
            }

            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int index = getIndex(key);
        AtomicReference<Node> head = buckets[index];

        while (true) {
            Node curr = head.get();

            while (curr != null) {
                AtomicMarkableReference<Node> next = curr.next;
                Node nextNode = next.getReference();

                if (curr.key == key) {
                    if (next.isMarked()) {
                        return false;
                    }
                    return true;
                }

                if (next.isMarked()) {
                    if (curr.next.compareAndSet(nextNode, nextNode)) {
                        continue;
                    }
                }

                curr = nextNode;
            }

            return false;
        }
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
    }
}