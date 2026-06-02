
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
        Node pred, curr;
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
            Node sentinel = new Node(Integer.MIN_VALUE + i, i, null);
            buckets.set(i, sentinel);
        }
    }

    private int getBucketIndex(int key, int cap) {
        return (key ^ (key >>> 16)) & (cap - 1);
    }

    private Node getSentinel(int index) {
        Node s = buckets.get(index);
        if (s == null) {
            Node newSentinel = new Node(Integer.MIN_VALUE + index, index, null);
            if (buckets.compareAndSet(index, null, newSentinel)) {
                return newSentinel;
            } else {
                return buckets.get(index);
            }
        }
        return s;
    }

    private Window find(Node head, int key) {
        Node pred, curr, succ;
        boolean[] marked = {false};

        retry:
        while (true) {
            pred = head;
            curr = pred.next.getReference();

            while (curr != null) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    boolean snip = pred.next.compareAndSet(curr, succ, false, false);
                    if (!snip) continue retry;
                    curr = succ;
                    if (curr == null) break;
                    succ = curr.next.get(marked);
                }
                if (curr == null) break;
                if (curr.key >= key) break;
                pred = curr;
                curr = succ;
            }
            return new Window(pred, curr);
        }
    }

    private void tryResize() {
        int cap = capacity.get();
        int sz = size.get();
        if (sz > cap * LOAD_FACTOR) {
            int newCap = cap << 1;
            if (capacity.compareAndSet(cap, newCap)) {
                AtomicReferenceArray<Node> newBuckets = new AtomicReferenceArray<>(newCap);
                AtomicReferenceArray<Node> old = this.buckets;
                for (int i = 0; i < cap; i++) {
                    newBuckets.set(i, old.get(i));
                }
                for (int i = cap; i < newCap; i++) {
                    Node sentinel = new Node(Integer.MIN_VALUE + i, i, null);
                    newBuckets.set(i, sentinel);
                }
                this.buckets = newBuckets;
            }
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            AtomicReferenceArray<Node> b = this.buckets;
            int cap = b.length();
            int idx = getBucketIndex(key, cap);
            Node head = getSentinel(idx);
            if (head == null) continue;

            Window w = find(head, key);
            Node pred = w.pred;
            Node curr = w.curr;

            if (curr != null && curr.key == key) {
                return false;
            }

            Node node = new Node(key, idx, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                int s = size.incrementAndGet();
                if (s > cap * LOAD_FACTOR) {
                    tryResize();
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            AtomicReferenceArray<Node> b = this.buckets;
            int cap = b.length();
            int idx = getBucketIndex(key, cap);
            Node head = getSentinel(idx);
            if (head == null) continue;

            Window w = find(head, key);
            Node pred = w.pred;
            Node curr = w.curr;

            if (curr == null || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            boolean marked = curr.next.compareAndSet(succ, succ, false, true);
            if (!marked) continue;
            // Node has been marked
            size.decrementAndGet();
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        AtomicReferenceArray<Node> b = this.buckets;
        int cap = b.length();
        int idx = getBucketIndex(key, cap);
        Node head = getSentinel(idx);
        if (head == null) return false;

        boolean[] marked = {false};
        Node curr = head.next.getReference();

        while (curr != null) {
            curr.next.get(marked);
            if (curr.key == key) {
                return !marked[0];
            }
            if (curr.key > key) return false;
            curr = curr.next.getReference();
        }
        return false;
    }
}
