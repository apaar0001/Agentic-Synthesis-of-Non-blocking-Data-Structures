package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReferenceArray;

public class ConcurrentDataStructure implements SetADT {
    private static final int INITIAL_CAPACITY = 16;
    private static final int MAX_CAPACITY = 1 << 30;
    private static final int HASH_BITS = 0x7fffffff;

    private static class Node {
        final int key;
        final AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private static class BucketList {
        final AtomicReferenceArray<Node> buckets;
        final AtomicInteger size;

        BucketList(int capacity) {
            this.buckets = new AtomicReferenceArray<>(capacity);
            this.size = new AtomicInteger(0);
            for (int i = 0; i < capacity; i++) {
                buckets.set(i, new Node(Integer.MIN_VALUE, null));
            }
        }

        Node getBucket(int index) {
            return buckets.get(index);
        }
    }

    private final AtomicReference<BucketList> table;
    private final AtomicInteger threshold;

    public ConcurrentDataStructure() {
        BucketList initial = new BucketList(INITIAL_CAPACITY);
        this.table = new AtomicReference<>(initial);
        this.threshold = new AtomicInteger(INITIAL_CAPACITY * 3 / 4);
    }

    private int hash(int key) {
        return (key ^ (key >>> 16)) & HASH_BITS;
    }

    private int bucketIndex(int hash, int capacity) {
        return hash & (capacity - 1);
    }

    private void maybeResize() {
        BucketList current = table.get();
        int currentSize = current.size.get();
        int currentCapacity = current.buckets.length();

        if (currentSize < threshold.get() || currentCapacity >= MAX_CAPACITY) {
            return;
        }

        int newCapacity = currentCapacity << 1;
        if (newCapacity > MAX_CAPACITY) {
            return;
        }

        BucketList newTable = new BucketList(newCapacity);
        if (table.compareAndSet(current, newTable)) {
            threshold.set(newCapacity * 3 / 4);
        }
    }

    @Override
    public boolean add(int key) {
        while (true) {
            BucketList currentTable = table.get();
            int hash = hash(key);
            int bucketIdx = bucketIndex(hash, currentTable.buckets.length());
            Node head = currentTable.getBucket(bucketIdx);

            Node pred = head;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);

                if (marked[0]) {
                    Node next = succ;
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
                } else {
                    if (curr.key == key) {
                        return false;
                    }
                    pred = curr;
                    curr = succ;
                }
            }

            Node newNode = new Node(key, null);
            if (pred.next.compareAndSet(null, newNode, false, false)) {
                currentTable.size.incrementAndGet();
                maybeResize();
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            BucketList currentTable = table.get();
            int hash = hash(key);
            int bucketIdx = bucketIndex(hash, currentTable.buckets.length());
            Node head = currentTable.getBucket(bucketIdx);

            Node pred = head;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);

                if (marked[0]) {
                    Node next = succ;
                    if (!pred.next.compareAndSet(curr, next, false, false)) {
                        break;
                    }
                    curr = next;
                } else {
                    if (curr.key == key) {
                        if (curr.next.compareAndSet(succ, succ, false, true)) {
                            // Node has been marked
                            currentTable.size.decrementAndGet();
                            pred.next.compareAndSet(curr, succ, false, false);
                            return true;
                        }
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        BucketList currentTable = table.get();
        int hash = hash(key);
        int bucketIdx = bucketIndex(hash, currentTable.buckets.length());
        Node head = currentTable.getBucket(bucketIdx);

        Node curr = head.next.getReference();
        while (curr != null) {
            boolean[] marked = {false};
            curr.next.get(marked);

            if (!marked[0] && curr.key == key) {
                return true;
            }
            curr = curr.next.getReference();
        }
        return false;
    }
}