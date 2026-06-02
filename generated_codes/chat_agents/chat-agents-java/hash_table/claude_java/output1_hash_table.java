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

    private final AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;
    private final AtomicInteger capacity;
    private final AtomicInteger count;

    public ConcurrentDataStructure() {
        this.buckets = new AtomicReferenceArray<>(INITIAL_CAPACITY * 2);
        this.size = new AtomicInteger(0);
        this.capacity = new AtomicInteger(INITIAL_CAPACITY);
        this.count = new AtomicInteger(0);
        Node sentinel = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null);
        buckets.set(0, sentinel);
    }

    private int splitOrderHash(int key) {
        return Integer.reverse(key & 0x7FFFFFFF);
    }

    private int sentinelHash(int bucketIndex) {
        return Integer.reverse(bucketIndex);
    }

    private Node getOrInitBucket(int index) {
        Node head = buckets.get(index);
        if (head != null) return head;

        int parent = index & (Integer.highestOneBit(index) - 1);
        Node parentHead = getOrInitBucket(parent);

        Node sentinel = new Node(Integer.MIN_VALUE + index, sentinelHash(index), null);
        Window window = find(parentHead, sentinelHash(index), Integer.MIN_VALUE + index);

        if (window.curr != null && window.curr.hash == sentinelHash(index) && window.curr.key == Integer.MIN_VALUE + index) {
            buckets.compareAndSet(index, null, window.curr);
        } else {
            Node newSentinel = new Node(Integer.MIN_VALUE + index, sentinelHash(index), window.curr);
            if (window.pred.next.compareAndSet(window.curr, newSentinel, false, false)) {
                buckets.compareAndSet(index, null, newSentinel);
            } else {
                Node existing = buckets.get(index);
                if (existing != null) return existing;
                return getOrInitBucket(index);
            }
        }

        head = buckets.get(index);
        return head != null ? head : parentHead;
    }

    private Window find(Node head, int hash, int key) {
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
        int hash = splitOrderHash(key);
        int cap = capacity.get();
        int bucketIndex = key & (cap - 1);

        Node head = getOrInitBucket(bucketIndex);

        while (true) {
            Window window = find(head, hash, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr != null && curr.hash == hash && curr.key == key) {
                return false;
            }

            Node node = new Node(key, hash, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                int newCount = count.incrementAndGet();
                int currentCap = capacity.get();
                if (newCount > currentCap * LOAD_FACTOR) {
                    capacity.compareAndSet(currentCap, currentCap * 2);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = splitOrderHash(key);
        int cap = capacity.get();
        int bucketIndex = key & (cap - 1);

        Node head = getOrInitBucket(bucketIndex);

        while (true) {
            Window window = find(head, hash, key);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr == null || curr.hash != hash || curr.key != key) {
                return false;
            }

            Node succ = curr.next.getReference();
            boolean marked = curr.next.compareAndSet(succ, succ, false, true);
            if (!marked) continue;
            // Node has been marked
            count.decrementAndGet();
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = splitOrderHash(key);
        int cap = capacity.get();
        int bucketIndex = key & (cap - 1);

        Node head = getOrInitBucket(bucketIndex);
        if (head == null) return false;

        boolean[] marked = {false};
        Node curr = head.next.getReference();

        while (curr != null) {
            if (curr.hash > hash || (curr.hash == hash && curr.key > key)) {
                return false;
            }
            curr.next.get(marked);
            if (curr.hash == hash && curr.key == key) {
                return !marked[0];
            }
            curr = curr.next.getReference();
        }

        return false;
    }
}