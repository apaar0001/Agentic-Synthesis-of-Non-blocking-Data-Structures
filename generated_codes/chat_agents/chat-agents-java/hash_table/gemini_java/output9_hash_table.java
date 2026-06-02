package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReferenceArray;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {

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

    private static final int MAX_LOAD = 2;

    private final AtomicReferenceArray<Node> buckets;
    private final AtomicInteger size;
    private final AtomicInteger bucketCount;
    private final Node head;

    public ConcurrentDataStructure() {
        this.head = new Node(0, reverseBits(0) | 1, null);
        this.buckets = new AtomicReferenceArray<>(8388608);
        this.buckets.set(0, head);
        this.size = new AtomicInteger(0);
        this.bucketCount = new AtomicInteger(1);
    }

    private static int reverseBits(int n) {
        int rev = Integer.reverse(n);
        return rev >>> 1;
    }

    private static int makeRegularHash(int key) {
        return reverseBits(key) & ~1;
    }

    private static int makeSentinelHash(int bucket) {
        return reverseBits(bucket) | 1;
    }

    private Window find(Node head, int hash) {
        Node pred;
        Node curr;
        Node succ;
        boolean[] marked = new boolean[1];
        boolean retry;

        retry_loop:
        while (true) {
            pred = head;
            curr = pred.next.getReference();
            while (true) {
                if (curr == null) {
                    return new Window(pred, curr);
                }
                succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        continue retry_loop;
                    }
                    curr = succ;
                    if (curr == null) {
                        return new Window(pred, curr);
                    }
                    succ = curr.next.get(marked);
                }
                if (Integer.compareUnsigned(curr.hash, hash) >= 0) {
                    return new Window(pred, curr);
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    private void initializeBucket(int bucket) {
        int parent = bucketCount.get();
        while (parent > bucket) {
            parent >>>= 1;
        }
        parent = bucket ^ Integer.highestOneBit(bucket);

        Node parentNode = buckets.get(parent);
        if (parentNode == null) {
            initializeBucket(parent);
            parentNode = buckets.get(parent);
        }

        int sentinelHash = makeSentinelHash(bucket);
        Window window = find(parentNode, sentinelHash);
        if (window.curr == null || window.curr.hash != sentinelHash) {
            Node sentinel = new Node(0, sentinelHash, window.curr);
            if (window.pred.next.compareAndSet(window.curr, sentinel, false, false)) {
                buckets.compareAndSet(bucket, null, sentinel);
            } else {
                initializeBucket(bucket);
            }
        } else {
            buckets.compareAndSet(bucket, null, window.curr);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = makeRegularHash(key);
        while (true) {
            int bCount = bucketCount.get();
            int bucket = key % bCount;
            Node bucketHead = buckets.get(bucket);
            if (bucketHead == null) {
                initializeBucket(bucket);
                bucketHead = buckets.get(bucket);
            }

            Window window = find(bucketHead, hash);
            if (window.curr != null && window.curr.hash == hash && window.curr.key == key) {
                return false;
            }

            Node node = new Node(key, hash, window.curr);
            if (window.pred.next.compareAndSet(window.curr, node, false, false)) {
                if (size.incrementAndGet() / bCount > MAX_LOAD) {
                    bucketCount.compareAndSet(bCount, bCount * 2);
                }
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = makeRegularHash(key);
        while (true) {
            int bCount = bucketCount.get();
            int bucket = key % bCount;
            Node bucketHead = buckets.get(bucket);
            if (bucketHead == null) {
                initializeBucket(bucket);
                bucketHead = buckets.get(bucket);
            }

            Window window = find(bucketHead, hash);
            if (window.curr == null || window.curr.hash != hash || window.curr.key != key) {
                return false;
            }

            Node succ = window.curr.next.getReference();
            if (window.curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                size.decrementAndGet();
                if (!window.pred.next.compareAndSet(window.curr, succ, false, false)) {
                    find(bucketHead, hash);
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = makeRegularHash(key);
        int bCount = bucketCount.get();
        int bucket = key % bCount;
        Node bucketHead = buckets.get(bucket);
        if (bucketHead == null) {
            initializeBucket(bucket);
            bucketHead = buckets.get(bucket);
        }

        Node curr = bucketHead;
        while (curr != null && Integer.compareUnsigned(curr.hash, hash) < 0) {
            curr = curr.next.getReference();
        }

        boolean[] marked = new boolean[1];
        if (curr != null) {
            curr.next.get(marked);
        }
        return curr != null && curr.hash == hash && curr.key == key && !marked[0];
    }
}