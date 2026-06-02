
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

    private final AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;
    private final AtomicInteger capacity;
    private final Node[] sentinels;

    public ConcurrentDataStructure() {
        this.capacity = new AtomicInteger(INITIAL_CAPACITY);
        this.buckets = new AtomicReferenceArray<>(INITIAL_CAPACITY * 8);
        this.size = new AtomicInteger(0);
        this.sentinels = new Node[INITIAL_CAPACITY * 8];
        Node sentinel = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null);
        sentinels[0] = sentinel;
        buckets.set(0, sentinel);
    }

    private int regularHash(int key) {
        int h = key ^ (key >>> 16);
        h = h & 0x7FFFFFFF;
        return h;
    }

    private int getBucket(int hash) {
        return hash % capacity.get();
    }

    private Node getOrInitBucket(int bucketIdx) {
        Node node = buckets.get(bucketIdx);
        if (node != null) return node;
        Node sentinel = new Node(Integer.MIN_VALUE + bucketIdx, Integer.MIN_VALUE + bucketIdx, null);
        if (buckets.compareAndSet(bucketIdx, null, sentinel)) {
            sentinels[bucketIdx] = sentinel;
            return sentinel;
        }
        return buckets.get(bucketIdx);
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
                if (curr.key >= key) return new Window(pred, curr);
                pred = curr;
                curr = succ;
            }
            return new Window(pred, curr);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = regularHash(key);
        while (true) {
            int bucketIdx = getBucket(hash);
            Node head = getOrInitBucket(bucketIdx);
            Window w = find(head, key);
            Node pred = w.pred;
            Node curr = w.curr;
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                int s = size.incrementAndGet();
                if (s > capacity.get() * LOAD_FACTOR) {
                    capacity.compareAndSet(capacity.get(), capacity.get() * 2);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = regularHash(key);
        while (true) {
            int bucketIdx = getBucket(hash);
            Node head = getOrInitBucket(bucketIdx);
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
        int hash = regularHash(key);
        int bucketIdx = getBucket(hash);
        Node head = buckets.get(bucketIdx);
        if (head == null) return false;
        boolean[] marked = {false};
        Node curr = head.next.getReference();
        while (curr != null) {
            curr.next.get(marked);
            if (!marked[0] && curr.key == key) return true;
            if (curr.key > key) return false;
            curr = curr.next.getReference();
        }
        return false;
    }
}
