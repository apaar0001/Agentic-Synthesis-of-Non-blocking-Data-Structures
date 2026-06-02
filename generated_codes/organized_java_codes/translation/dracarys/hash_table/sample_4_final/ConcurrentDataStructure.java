package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int INITIAL_CAPACITY = 16;
    private static final int MAX_CAPACITY = 1 << 30;
    private static final float LOAD_FACTOR = 0.75f;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static class Bucket {
        private final AtomicReference<Node> head;

        Bucket() {
            head = new AtomicReference<>(null);
        }
    }

    private final Bucket[] buckets;
    private final AtomicInteger size;
    private final AtomicInteger threshold;

    public ConcurrentDataStructure() {
        buckets = new Bucket[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            buckets[i] = new Bucket();
        }
        size = new AtomicInteger(0);
        threshold = new AtomicInteger((int) (INITIAL_CAPACITY * LOAD_FACTOR));
    }

    private int hash(int key) {
        return Math.abs(key) % buckets.length;
    }

    private boolean add(int key, Node[] preds, Node[] succs) {
        int index = hash(key);
        Node pred = preds[index];
        Node succ = succs[index];
        if (succ != null && succ.key == key) {
            return false;
        }
        Node newNode = new Node(key, succ);
        if (pred.next.compareAndSet(succ, newNode, false, false)) {
            size.incrementAndGet();
            checkResize();
            return true;
        }
        return false;
    }

    private boolean remove(int key, Node[] preds, Node[] succs) {
        int index = hash(key);
        Node pred = preds[index];
        Node succ = succs[index];
        if (succ == null || succ.key != key) {
            return false;
        }
        Node next = succ.next.getReference();
        if (pred.next.compareAndSet(succ, next, false, false)) {
            succ.next.set(next, true); // Node has been marked
            size.decrementAndGet();
            return true;
        }
        return false;
    }

    private boolean contains(int key, Node[] preds, Node[] succs) {
        int index = hash(key);
        Node pred = preds[index];
        Node succ = succs[index];
        if (succ != null && succ.key == key && !succ.next.isMarked()) {
            return true;
        }
        return false;
    }

    private void checkResize() {
        if (size.get() > threshold.get()) {
            resize();
        }
    }

    private void resize() {
        int newCapacity = Math.min(buckets.length << 1, MAX_CAPACITY);
        Bucket[] newBuckets = new Bucket[newCapacity];
        for (int i = 0; i < newCapacity; i++) {
            newBuckets[i] = new Bucket();
        }
        for (int i = 0; i < buckets.length; i++) {
            Node node = buckets[i].head.get();
            while (node != null) {
                int newIndex = Math.abs(node.key) % newCapacity;
                Node next = node.next.getReference();
                node.next.set(newBuckets[newIndex].head.get(), false);
                newBuckets[newIndex].head.compareAndSet(newBuckets[newIndex].head.get(), node, false, false);
                node = next;
            }
        }
        buckets = newBuckets;
        threshold.set((int) (newCapacity * LOAD_FACTOR));
    }

    @Override
    public boolean add(int key) {
        while (true) {
            Node[] preds = new Node[buckets.length];
            Node[] succs = new Node[buckets.length];
            for (int i = 0; i < buckets.length; i++) {
                preds[i] = buckets[i].head.get();
                succs[i] = null;
            }
            for (int i = 0; i < buckets.length; i++) {
                Node node = preds[i];
                while (node != null) {
                    if (node.next.isMarked()) {
                        node = node.next.getReference();
                    } else {
                        succs[i] = node;
                        break;
                    }
                }
            }
            if (add(key, preds, succs)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            Node[] preds = new Node[buckets.length];
            Node[] succs = new Node[buckets.length];
            for (int i = 0; i < buckets.length; i++) {
                preds[i] = buckets[i].head.get();
                succs[i] = null;
            }
            for (int i = 0; i < buckets.length; i++) {
                Node node = preds[i];
                while (node != null) {
                    if (node.next.isMarked()) {
                        node = node.next.getReference();
                    } else {
                        succs[i] = node;
                        break;
                    }
                }
            }
            if (remove(key, preds, succs)) {
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        while (true) {
            Node[] preds = new Node[buckets.length];
            Node[] succs = new Node[buckets.length];
            for (int i = 0; i < buckets.length; i++) {
                preds[i] = buckets[i].head.get();
                succs[i] = null;
            }
            for (int i = 0; i < buckets.length; i++) {
                Node node = preds[i];
                while (node != null) {
                    if (node.next.isMarked()) {
                        node = node.next.getReference();
                    } else {
                        succs[i] = node;
                        break;
                    }
                }
            }
            if (contains(key, preds, succs)) {
                return true;
            }
        }
    }
}