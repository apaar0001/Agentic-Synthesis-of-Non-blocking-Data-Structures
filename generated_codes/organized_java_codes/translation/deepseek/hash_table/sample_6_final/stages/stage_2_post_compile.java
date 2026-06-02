package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int INITIAL_CAPACITY = 16;
    private static final float LOAD_FACTOR = 0.75f;
    private final AtomicReference<Node>[] buckets;
    private final AtomicInteger size = new AtomicInteger(0);
    private final AtomicInteger capacity;

    private static class Node {
        final int key;
        final int hash;
        final AtomicMarkableReference<Node> next;

        Node(int key, int hash, Node next) {
            this.key = key;
            this.hash = hash;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        capacity = new AtomicInteger(INITIAL_CAPACITY);
        buckets = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            buckets[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return key;
    }

    private int bucketIndex(int hash) {
        return Math.abs(hash) % capacity.get();
    }

    private void maybeResize() {
        int currentSize = size.get();
        int currentCap = capacity.get();
        if (currentSize > currentCap * LOAD_FACTOR) {
            int newCap = currentCap * 2;
            if (capacity.compareAndSet(currentCap, newCap)) {
                @SuppressWarnings("unchecked")
                AtomicReference<Node>[] newBuckets = new AtomicReference[newCap];
                for (int i = 0; i < newCap; i++) {
                    newBuckets[i] = new AtomicReference<>(null);
                }
                for (int i = 0; i < currentCap; i++) {
                    Node node = buckets[i].get();
                    while (node != null) {
                        int newIdx = Math.abs(node.hash) % newCap;
                        Node newNode = new Node(node.key, node.hash, newBuckets[newIdx].get());
                        newBuckets[newIdx].set(newNode);
                        node = getNext(node);
                    }
                }
                for (int i = 0; i < newCap; i++) {
                    buckets[i] = newBuckets[i];
                }
            }
        }
    }

    private Node getNext(Node node) {
        Node next;
        boolean[] mark = new boolean[1];
        do {
            next = node.next.get(mark);
        } while (mark[0]);
        return next;
    }

    @Override
    public boolean add(int key) {
        int hash = hash(key);
        while (true) {
            int idx = bucketIndex(hash);
            AtomicReference<Node> bucket = buckets[idx];
            Node head = bucket.get();
            Node pred = null;
            Node curr = head;
            while (curr != null) {
                Node next = getNext(curr);
                if (curr.key == key) {
                    return false;
                }
                pred = curr;
                curr = next;
            }
            Node newNode = new Node(key, hash, head);
            if (bucket.compareAndSet(head, newNode)) {
                size.incrementAndGet();
                maybeResize();
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = hash(key);
        while (true) {
            int idx = bucketIndex(hash);
            AtomicReference<Node> bucket = buckets[idx];
            Node head = bucket.get();
            Node pred = null;
            Node curr = head;
            while (curr != null) {
                Node next = getNext(curr);
                if (curr.key == key) {
                    if (curr.next.compareAndSet(next, next, false, true)) {
                        // Node has been marked
                        bucket.compareAndSet(head, getNext(curr));
                        size.decrementAndGet();
                        return true;
                    }
                    break;
                }
                pred = curr;
                curr = next;
            }
            if (curr == null) {
                return false;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = hash(key);
        int idx = bucketIndex(hash);
        Node curr = buckets[idx].get();
        while (curr != null) {
            if (curr.key == key) {
                boolean[] mark = new boolean[1];
                curr.next.get(mark);
                return !mark[0];
            }
            curr = getNext(curr);
        }
        return false;
    }
}