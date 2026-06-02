package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
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

    private static final int INITIAL_CAPACITY = 16;
    private final AtomicReference<Node[]> buckets;
    private final AtomicInteger size;
    private final Node head;

    public ConcurrentDataStructure() {
        this.head = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null);
        Node[] initialBuckets = new Node[INITIAL_CAPACITY];
        initialBuckets[0] = this.head;
        this.buckets = new AtomicReference<>(initialBuckets);
        this.size = new AtomicInteger(0);
    }

    private int makeRegularHash(int key) {
        int h = Integer.hashCode(key);
        h ^= (h >>> 16);
        return h & 0x7FFFFFFF;
    }

    private int makeSentinelHash(int bucket) {
        int rev = Integer.reverse(bucket);
        return rev >>> 1;
    }

    private int makeRegularHashFromBucket(int key) {
        int rev = Integer.reverse(makeRegularHash(key));
        return (rev >>> 1) | 0x80000000;
    }

    private Window find(Node head, int hash) {
        Node pred;
        Node curr;
        Node succ;
        boolean[] marked = {false};
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
        Node[] currentBuckets = buckets.get();
        if (currentBuckets[bucket] != null) {
            return;
        }
        int parent = bucket;
        while (parent >= currentBuckets.length) {
            checkAndResize();
            currentBuckets = buckets.get();
        }
        parent = Integer.highestOneBit(bucket) - 1;
        if (parent < 0) parent = 0;
        while (currentBuckets[parent] == null) {
            initializeBucket(parent);
        }
        int sentinelHash = makeSentinelHash(bucket);
        Node sentinel = new Node(0, sentinelHash, null);
        while (true) {
            Window window = find(currentBuckets[parent], sentinelHash);
            if (window.curr != null && window.curr.hash == sentinelHash) {
                currentBuckets[bucket] = window.curr;
                return;
            }
            sentinel.next.set(window.curr, false);
            if (window.pred.next.compareAndSet(window.curr, sentinel, false, false)) {
                currentBuckets[bucket] = sentinel;
                return;
            }
        }
    }

    private void checkAndResize() {
        Node[] currentBuckets = buckets.get();
        int currentSize = size.get();
        if (currentSize > currentBuckets.length * 2) {
            int newLength = currentBuckets.length * 2;
            Node[] newBuckets = new Node[newLength];
            System.arraycopy(currentBuckets, 0, newBuckets, 0, currentBuckets.length);
            buckets.compareAndSet(currentBuckets, newBuckets);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = makeRegularHashFromBucket(key);
        Node node = new Node(key, hash, null);
        Node[] currentBuckets = buckets.get();
        int bucket = makeRegularHash(key) % currentBuckets.length;
        if (currentBuckets[bucket] == null) {
            initializeBucket(bucket);
            currentBuckets = buckets.get();
        }
        while (true) {
            Window window = find(currentBuckets[bucket], hash);
            if (window.curr != null && window.curr.hash == hash) {
                return false;
            }
            node.next.set(window.curr, false);
            if (window.pred.next.compareAndSet(window.curr, node, false, false)) {
                size.incrementAndGet();
                checkAndResize();
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = makeRegularHashFromBucket(key);
        Node[] currentBuckets = buckets.get();
        int bucket = makeRegularHash(key) % currentBuckets.length;
        if (currentBuckets[bucket] == null) {
            initializeBucket(bucket);
            currentBuckets = buckets.get();
        }
        while (true) {
            Window window = find(currentBuckets[bucket], hash);
            if (window.curr == null || window.curr.hash != hash) {
                return false;
            }
            Node succ = window.curr.next.getReference();
            if (window.curr.next.compareAndSet(succ, succ, false, true)) {
                // Node has been marked
                size.decrementAndGet();
                window.pred.next.compareAndSet(window.curr, succ, false, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = makeRegularHashFromBucket(key);
        Node[] currentBuckets = buckets.get();
        int bucket = makeRegularHash(key) % currentBuckets.length;
        if (currentBuckets[bucket] == null) {
            initializeBucket(bucket);
            currentBuckets = buckets.get();
        }
        Node curr = currentBuckets[bucket];
        while (curr != null && Integer.compareUnsigned(curr.hash, hash) < 0) {
            curr = curr.next.getReference();
        }
        return curr != null && curr.hash == hash && !curr.next.isMarked();
    }
}