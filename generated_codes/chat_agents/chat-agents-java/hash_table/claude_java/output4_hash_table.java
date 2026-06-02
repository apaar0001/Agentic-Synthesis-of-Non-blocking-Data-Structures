package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {

    private static final int INITIAL_CAPACITY = 16;
    private static final double LOAD_FACTOR = 2.0;

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

    private static class Window {
        final Node pred;
        final Node curr;
        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private volatile AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;
    private final AtomicInteger capacity;

    public ConcurrentDataStructure() {
        int cap = INITIAL_CAPACITY;
        this.buckets = new AtomicReferenceArray<>(cap);
        this.size = new AtomicInteger(0);
        this.capacity = new AtomicInteger(cap);
        for (int i = 0; i < cap; i++) {
            buckets.set(i, new Node(Integer.MIN_VALUE + i, Integer.MIN_VALUE + i, null));
        }
    }

    private int spreadHash(int key) {
        int h = key ^ (key >>> 16);
        return h & 0x7FFFFFFF;
    }

    private int getBucketIndex(int hash, int cap) {
        return hash % cap;
    }

    private Node getSentinel(int bucketIndex) {
        Node sentinel = buckets.get(bucketIndex);
        if (sentinel == null) {
            Node newSentinel = new Node(Integer.MIN_VALUE + bucketIndex, Integer.MIN_VALUE + bucketIndex, null);
            if (buckets.compareAndSet(bucketIndex, null, newSentinel)) {
                sentinel = newSentinel;
            } else {
                sentinel = buckets.get(bucketIndex);
            }
        }
        return sentinel;
    }

    private Window find(Node head, int hash, int key) {
        Node pred, curr, succ;
        boolean[] marked = {false};
        boolean snip;

        retry:
        while (true) {
            pred = head;
            curr = pred.next.getReference();

            while (curr != null) {
                succ = curr.next.get(marked);

                while (marked[0]) {
                    snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) continue retry;
                    curr = succ;
                    if (curr == null) break;
                    succ = curr.next.get(marked);
                }

                if (curr == null) break;

                if (curr.hash > hash || (curr.hash == hash && curr.key >= key)) {
                    return new Window(pred, curr);
                }

                pred = curr;
                curr = succ;
            }

            return new Window(pred, curr);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = spreadHash(key);

        while (true) {
            int cap = capacity.get();
            int bucketIndex = getBucketIndex(hash, cap);
            Node head = getSentinel(bucketIndex);

            Window window = find(head, hash, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr != null && curr.hash == hash && curr.key == key) {
                return false;
            }

            Node newNode = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, newNode, false, false)) {
                int newSize = size.incrementAndGet();
                if ((double) newSize / cap > LOAD_FACTOR) {
                    tryResize(cap);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = spreadHash(key);

        while (true) {
            int cap = capacity.get();
            int bucketIndex = getBucketIndex(hash, cap);
            Node head = getSentinel(bucketIndex);

            Window window = find(head, hash, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr == null || curr.hash != hash || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            boolean marked = curr.next.compareAndSet(succ, succ, false, true);
            if (!marked) {
                continue;
            }
            // Node has been marked
            size.decrementAndGet();
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = spreadHash(key);
        int cap = capacity.get();
        int bucketIndex = getBucketIndex(hash, cap);
        Node head = getSentinel(bucketIndex);

        boolean[] marked = {false};
        Node curr = head.next.getReference();

        while (curr != null) {
            if (curr.hash == hash && curr.key == key) {
                curr.next.get(marked);
                return !marked[0];
            }
            if (curr.hash > hash || (curr.hash == hash && curr.key > key)) {
                return false;
            }
            curr = curr.next.getReference();
        }

        return false;
    }

    private void tryResize(int oldCap) {
        int newCap = oldCap * 2;
        if (!capacity.compareAndSet(oldCap, newCap)) {
            return;
        }

        AtomicReferenceArray<Node> newBuckets = new AtomicReferenceArray<>(newCap);
        AtomicReferenceArray<Node> oldBuckets = this.buckets;

        for (int i = 0; i < oldCap; i++) {
            Node sentinel = new Node(Integer.MIN_VALUE + i, Integer.MIN_VALUE + i, null);
            newBuckets.set(i, sentinel);
        }
        for (int i = oldCap; i < newCap; i++) {
            Node sentinel = new Node(Integer.MIN_VALUE + i, Integer.MIN_VALUE + i, null);
            newBuckets.set(i, sentinel);
        }

        for (int i = 0; i < oldCap; i++) {
            Node oldHead = oldBuckets.get(i);
            if (oldHead == null) continue;

            boolean[] marked = {false};
            Node curr = oldHead.next.getReference();

            while (curr != null) {
                Node succ = curr.next.get(marked);
                if (!marked[0]) {
                    int newBucketIndex = getBucketIndex(curr.hash, newCap);
                    Node newHead = newBuckets.get(newBucketIndex);
                    if (newHead != null) {
                        Node expected = newHead.next.getReference();
                        Node newNode = new Node(curr.key, curr.hash, expected);
                        newHead.next.compareAndSet(expected, newNode, false, false);
                    }
                }
                curr = succ;
            }
        }

        this.buckets = newBuckets;
    }
}