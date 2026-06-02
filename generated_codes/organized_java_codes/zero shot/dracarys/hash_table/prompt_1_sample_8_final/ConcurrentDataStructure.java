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
        Node newNode = new Node(key);

        while (true) {
            Node curr = buckets[index].get();
            if (curr == null) {
                if (buckets[index].compareAndSet(null, newNode)) {
                    return true;
                }
            } else {
                if (curr.key == key) {
                    return false;
                }
                AtomicMarkableReference<Node> next = curr.next;
                Node nextNode = next.getReference();
                if (nextNode == null) {
                    if (curr.next.compareAndSet(null, newNode)) {
                        return true;
                    }
                } else {
                    if (next.isMarked()) {
                        if (curr.next.compareAndSet(nextNode, nextNode.next.getReference())) {
                            continue;
                        }
                    } else {
                        if (buckets[index].compareAndSet(curr, nextNode)) {
                            continue;
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int index = getIndex(key);

        while (true) {
            Node curr = buckets[index].get();
            if (curr == null) {
                return false;
            }

            AtomicMarkableReference<Node> next = curr.next;
            Node nextNode = next.getReference();
            if (nextNode == null) {
                return false;
            }

            if (nextNode.key == key) {
                if (next.isMarked()) {
                    return false;
                }
                if (next.attemptMark(nextNode, true)) {
                    // Node has been marked
                    if (curr.next.compareAndSet(nextNode, nextNode.next.getReference())) {
                        return true;
                    }
                }
            } else {
                if (next.isMarked()) {
                    if (curr.next.compareAndSet(nextNode, nextNode.next.getReference())) {
                        continue;
                    }
                } else {
                    if (buckets[index].compareAndSet(curr, nextNode)) {
                        continue;
                    }
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int index = getIndex(key);

        while (true) {
            Node curr = buckets[index].get();
            if (curr == null) {
                return false;
            }

            AtomicMarkableReference<Node> next = curr.next;
            Node nextNode = next.getReference();
            if (nextNode == null) {
                return false;
            }

            if (nextNode.key == key) {
                if (next.isMarked()) {
                    return false;
                }
                return true;
            } else {
                if (next.isMarked()) {
                    if (curr.next.compareAndSet(nextNode, nextNode.next.getReference())) {
                        continue;
                    }
                } else {
                    if (buckets[index].compareAndSet(curr, nextNode)) {
                        continue;
                    }
                }
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
    }
}