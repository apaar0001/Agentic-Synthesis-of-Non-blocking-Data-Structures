```java
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
    private static final double LOAD_FACTOR = 0.75;

    private final AtomicReference<AtomicReference<Node>[]> buckets;
    private final AtomicInteger size;
    private final Node head;

    public ConcurrentDataStructure() {
        this.head = new Node(0, Integer.MIN_VALUE, null);
        @SuppressWarnings("unchecked")
        AtomicReference<Node>[] initialBuckets = new AtomicReference[INITIAL_CAPACITY];
        for (int i = 0; i < INITIAL_CAPACITY; i++) {
            initialBuckets[i] = new AtomicReference<>(null);
        }
        initialBuckets[0] = new AtomicReference<>(head);
        this.buckets = new AtomicReference<>(initialBuckets);
        this.size = new AtomicInteger(0);
    }

    private int makeRegularKey(int key) {
        return Integer.reverse(key) | 1;
    }

    private int makeSentinelKey(int key) {
        return Integer.reverse(key) & ~1;
    }

    private Window find(Node head, int hash) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        boolean retry;

        while (true) {
            retry = false;
            pred = head;
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
                if (curr == null || Integer.compareUnsigned(curr.hash, hash) >= 0) {
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

    private Node initializeBucket(int bucketIndex) {
        AtomicReference<Node>[] currentBuckets = buckets.get();
        if (currentBuckets[bucketIndex] != null) {
            return currentBuckets[bucketIndex].get();
        }

        int parentBucket = bucketIndex ^ Integer.highestOneBit(bucketIndex);
        if (currentBuckets[parentBucket] == null) {
            initializeBucket(parentBucket);
        }

        Node parentNode = currentBuckets[parentBucket].get();
        int sentinelHash = makeSentinelKey(bucketIndex);

        while (true) {
            Window window = find(parentNode, sentinelHash);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr != null && curr.hash == sentinelHash) {
                currentBuckets[bucketIndex] = new AtomicReference<>(curr);
                return curr;
            } else {
                Node node = new Node(0, sentinelHash, curr);
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    currentBuckets[bucketIndex] = new AtomicReference<>(node);
                    return node;
                }
            }
        }
    }

    private void checkSizeAndResize() {
        AtomicReference<Node>[] currentBuckets = buckets.get();
        int currentCapacity = currentBuckets.length;
        if ((double) size.get() / currentCapacity > LOAD_FACTOR) {
            int newCapacity = currentCapacity * 2;
            @SuppressWarnings("unchecked")
            AtomicReference<Node>[] newBuckets = new AtomicReference[newCapacity];
            System.arraycopy(currentBuckets, 0, newBuckets, 0, currentCapacity);
            for (int i = currentCapacity; i < newCapacity; i++) {
                newBuckets[i] = null;
            }
            buckets.compareAndSet(currentBuckets, newBuckets);
        }
    }

    @Override
    public boolean add(int key) {
        int hash = makeRegularKey(key);
        AtomicReference<Node>[] currentBuckets = buckets.get();
        int bucketIndex = key % currentBuckets.length;
        if (bucketIndex < 0) {
            bucketIndex += currentBuckets.length;
        }

        if (currentBuckets[bucketIndex] == null) {
            initializeBucket(bucketIndex);
        }

        Node bucketNode = buckets.get()[bucketIndex].get();

        while (true) {
            Window window = find(bucketNode, hash);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr != null && curr.hash == hash) {
                return false;
            } else {
                Node node = new Node(key, hash, curr);
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    size.incrementAndGet();
                    checkSizeAndResize();
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int hash = makeRegularKey(key);
        AtomicReference<Node>[] currentBuckets = buckets.get();
        int bucketIndex = key % currentBuckets.length;
        if (bucketIndex < 0) {
            bucketIndex += currentBuckets.length;
        }

        if (currentBuckets[bucketIndex] == null) {
            initializeBucket(bucketIndex);
        }

        Node bucketNode = buckets.get()[bucketIndex].get();

        while (true) {
            Window window = find(bucketNode, hash);
            Node pred = window.pred;
            Node curr = window.curr;

            if (curr == null || curr.hash != hash) {
                return false;
            } else {
                Node succ = curr.next.getReference();
                if (curr.next.compareAndSet(succ, succ, false, true)) {
                    // Node has been marked
                    pred.next.compareAndSet(curr, succ, false, false);
                    size.decrementAndGet();
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int hash = makeRegularKey(key);
        AtomicReference<Node>[] currentBuckets = buckets.get();
        int bucketIndex = key % currentBuckets.length;
        if (bucketIndex < 0) {
            bucketIndex += currentBuckets.length;
        }

        if (currentBuckets[bucketIndex] == null) {
            initializeBucket(bucketIndex);
        }

        Node bucketNode = buckets.get()[bucketIndex].get();
        Node curr = bucketNode.next.getReference();
        boolean[] marked = {false};

        while (curr != null && Integer.compareUnsigned(curr.hash, hash) < 0) {
            curr = curr.next.getReference();
        }

        if (curr != null && curr.hash == hash) {
            curr.next.get(marked);
            return !marked[0];
        }
        return false;
    }
}

```