package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

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
        Node pred;
        Node curr;

        Window(Node pred, Node curr) {
            this.pred = pred;
            this.curr = curr;
        }
    }

    private static final int INITIAL_CAPACITY = 16;
    private final AtomicReferenceArray<Node> buckets;
    private final Node head;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        this.buckets = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        this.head = new Node(Integer.MIN_VALUE, Integer.MIN_VALUE, null);
        this.buckets.set(0, head);
        this.size = new AtomicInteger(0);
    }

    private int makeRegularKey(int key) {
        return Integer.reverse(key) | 1;
    }

    private int makeSentinelKey(int key) {
        return Integer.reverse(key) & ~1;
    }

    private Window find(Node start, int hash) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        boolean retry;

        while (true) {
            retry = false;
            pred = start;
            curr = pred.next.getReference();

            while (curr != null) {
                succ = curr.next.get(marked);
                while (marked[0]) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        retry = true;
                        break;
                    }
                    curr = succ;
                    if (curr == null) {
                        break;
                    }
                    succ = curr.next.get(marked);
                }

                if (retry) {
                    break;
                }

                if (curr == null || curr.hash >= hash) {
                    return new Window(pred, curr);
                }

                pred = curr;
                curr = curr.next.getReference();
            }

            if (!retry) {
                return new Window(pred, curr);
            }
        }
    }

    private void initializeBucket(int bucket) {
        int parent = Integer.highestOneBit(bucket) >> 1;
        if (parent >= bucket) {
            parent = 0;
        }

        if (buckets.get(parent) == null) {
            initializeBucket(parent);
        }

        Node parentNode = buckets.get(parent);
        int sentinelHash = makeSentinelKey(bucket);
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

    private Node getBucketNode(int key) {
        int bucket = key % buckets.length();
        if (bucket < 0) {
            bucket += buckets.length();
        }

        Node bucketNode = buckets.get(bucket);
        if (bucketNode == null) {
            initializeBucket(bucket);
            bucketNode = buckets.get(bucket);
        }
        return bucketNode;
    }

    @Override
    public boolean add(int key) {
        int hash = makeRegularKey(key);
        Node bucketNode = getBucketNode(key);

        while (true) {
            Window window = find(bucketNode, hash);
            if (window.curr != null && window.curr.hash == hash) {
                return false;
            }

            Node newNode = new Node(key, hash, window.curr);
            if (window.pred.next.compareAndSet(window.curr, newNode, false, false)) {
                size.incrementAndGet();
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = makeRegularKey(key);
        Node bucketNode = getBucketNode(key);

        while (true) {
            Window window = find(bucketNode, hash);
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
        int hash = makeRegularKey(key);
        Node bucketNode = getBucketNode(key);
        boolean[] marked = {false};

        Node curr = bucketNode;
        while (curr != null && curr.hash < hash) {
            curr = curr.next.getReference();
        }

        if (curr == null || curr.hash != hash) {
            return false;
        }

        curr.next.get(marked);
        return !marked[0];
    }
}