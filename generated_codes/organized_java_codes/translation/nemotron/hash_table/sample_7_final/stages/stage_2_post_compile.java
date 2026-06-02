package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static final int DEFAULT_CAPACITY = 16;
    private final AtomicReference<Node>[] table;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        table = new AtomicReferenceReference[DEFAULT_CAPACITY];
        for (int i = 0; i < DEFAULT_CAPACITY; i++) {
            table[i] = new AtomicReference<>(null);
        }
    }

    private static int hash(int key) {
        return Integer.hashCode(key);
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        while (true) {
            Node pred = null;
            Node curr = bucket.get();
            while (true) {
                if (curr == null) {
                    Node newNode = new Node(key, null);
                    if (pred == null) {
                        if (bucket.compareAndSet(null, newNode)) {
                            return true;
                        } else {
                            break;
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, newNode, false, false)) {
                            return true;
                        } else {
                            break;
                        }
                    }
                }
                if (curr.next.isMarked()) {
                    curr = curr.next.getReference();
                    continue;
                }
                int k = curr.key;
                if (k == key) {
                    return false;
                } else if (k > key) {
                    Node newNode = new Node(key, curr);
                    if (pred == null) {
                        if (bucket.compareAndSet(curr, newNode)) {
                            return true;
                        } else {
                            break;
                        }
                    } else {
                        if (pred.next.compareAndSet(curr, newNode, false, false)) {
                            return true;
                        } else {
                            break;
                        }
                    }
                } else {
                    pred = curr;
                    curr = curr.next.getReference();
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        while (true) {
            Node pred = null;
            Node curr = bucket.get();
            while (true) {
                if (curr == null) {
                    return false;
                }
                if (curr.next.isMarked()) {
                    curr = curr.next.getReference();
                    continue;
                }
                int k = curr.key;
                if (k == key) {
                    if (curr.next.attemptMark(curr.next.getReference(), true)) {
                        // Node has been marked
                        return true;
                    } else {
                        break;
                    }
                } else if (k > key) {
                    return false;
                } else {
                    pred = curr;
                    curr = curr.next.getReference();
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key) & (table.length - 1);
        AtomicReference<Node> bucket = table[idx];
        Node curr = bucket.get();
        while (curr != null) {
            if (curr.next.isMarked()) {
                curr = curr.next.getReference();
                continue;
            }
            int k = curr.key;
            if (k == key) {
                return true;
            } else if (k > key) {
                return false;
            } else {
                curr = curr.next.getReference();
            }
        }
        return false;
    }
}