package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        public Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private static final int CAPACITY = 10;
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
        AtomicReference<Node> bucket = buckets[index];
        Node newNode = new Node(key);

        while (true) {
            Node curr = bucket.get();
            Node next = curr == null ? null : curr.next.getReference();

            if (curr == null) {
                if (bucket.compareAndSet(null, newNode)) {
                    return true;
                }
            } else if (next == null) {
                if (curr.next.compareAndSet(null, newNode)) {
                    return true;
                }
            } else if (next.key == key) {
                return false;
            } else if (next.next.getReference() == null) {
                if (next.next.compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                bucket.compareAndSet(curr, next);
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = index(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            Node next = curr == null ? null : curr.next.getReference();

            if (curr == null) {
                return false;
            } else if (next == null) {
                if (curr.key == key) {
                    if (curr.next.compareAndSet(null, curr)) {
                        curr.next.set(null, true);
                        // Node has been marked
                        return true;
                    }
                } else {
                    return false;
                }
            } else if (next.key == key) {
                if (next.next.getReference() == null) {
                    if (curr.next.compareAndSet(next, next.next.getReference())) {
                        next.next.set(null, true);
                        // Node has been marked
                        return true;
                    }
                } else {
                    if (next.next.compareAndSet(next.next.getReference(), next.next.getReference())) {
                        next.next.set(null, true);
                        // Node has been marked
                        return true;
                    }
                }
            } else {
                bucket.compareAndSet(curr, next);
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = index(key);
        AtomicReference<Node> bucket = buckets[index];

        while (true) {
            Node curr = bucket.get();
            Node next = curr == null ? null : curr.next.getReference();

            if (curr == null) {
                return false;
            } else if (next == null) {
                if (curr.key == key) {
                    return true;
                } else {
                    return false;
                }
            } else if (next.key == key) {
                if (next.next.getReference() == null) {
                    return true;
                } else {
                    if (curr.next.compareAndSet(next, next.next.getReference())) {
                        next.next.set(null, true);
                        // Node has been marked
                    }
                    return false;
                }
            } else {
                bucket.compareAndSet(curr, next);
            }
        }
    }
}