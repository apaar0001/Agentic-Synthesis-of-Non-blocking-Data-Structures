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

    private AtomicReference<Node>[] buckets;
    private int capacity;

    public ConcurrentDataStructure() {
        capacity = 10;
        buckets = new AtomicReference[capacity];

        for (int i = 0; i < capacity; i++) {
            buckets[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int index = hash(key);
        Node curr = buckets[index].get();
        Node prev = null;

        while (curr != null) {
            if (curr.key == key) {
                return false;
            }
            prev = curr;
            curr = curr.next.getReference();
        }

        Node newNode = new Node(key);
        newNode.next.set(prev, false);

        while (!buckets[index].compareAndSet(prev, newNode)) {
            curr = buckets[index].get();
            if (curr == null) {
                return false;
            }
            prev = curr;
            while (curr.next.isMarked()) {
                curr = curr.next.getReference();
            }
        }

        return true;
    }

    @Override
    public boolean remove(int key) {
        int index = hash(key);
        Node curr = buckets[index].get();
        Node prev = null;

        while (curr != null) {
            if (curr.key == key) {
                if (curr.next.isMarked()) {
                    return false;
                }
                if (curr.next.compareAndSet(null, null, false, true)) {
                    // Node has been marked
                    return true;
                }
            }
            prev = curr;
            curr = curr.next.getReference();
        }

        return false;
    }

    @Override
    public boolean contains(int key) {
        int index = hash(key);
        Node curr = buckets[index].get();

        while (curr != null) {
            if (curr.key == key) {
                if (curr.next.isMarked()) {
                    return false;
                }
                return true;
            }
            curr = curr.next.getReference();
        }

        return false;
    }
}