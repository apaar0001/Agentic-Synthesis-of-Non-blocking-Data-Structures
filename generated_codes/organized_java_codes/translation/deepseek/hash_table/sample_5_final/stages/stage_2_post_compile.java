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
        final int hash;
        final AtomicMarkableReference<Node> next;

        Node(int key, int hash, Node next) {
            this.key = key;
            this.hash = hash;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final AtomicReferenceArray<Node> table;
    private final AtomicInteger size;

    public ConcurrentDataStructure() {
        this.table = new AtomicReferenceArray<>(INITIAL_CAPACITY);
        this.size = new AtomicInteger(0);
    }

    private static int spread(int h) {
        return (h ^ (h >>> 16)) & HASH_BITS;
    }

    private Node findBucket(int hash) {
        int index = (table.length() - 1) & hash;
        Node dummy = new Node(-1, hash, null);
        while (true) {
            Node current = table.get(index);
            if (current != null) {
                return current;
            }
            if (table.compareAndSet(index, null, dummy)) {
                return dummy;
            }
        }
    }

    private boolean addInternal(int key) {
        int hash = spread(key);
        while (true) {
            Node bucketHead = findBucket(hash);
            Node pred = bucketHead;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node nextSucc = succ;
                    if (!pred.next.compareAndSet(curr, nextSucc, false, false)) {
                        break;
                    }
                    curr = nextSucc;
                } else {
                    if (curr.key == key) {
                        return false;
                    }
                    pred = curr;
                    curr = succ;
                }
            }
            Node newNode = new Node(key, hash, null);
            if (pred.next.compareAndSet(null, newNode, false, false)) {
                size.incrementAndGet();
                return true;
            }
        }
    }

    private boolean removeInternal(int key) {
        int hash = spread(key);
        while (true) {
            Node bucketHead = findBucket(hash);
            Node pred = bucketHead;
            Node curr = pred.next.getReference();
            while (curr != null) {
                boolean[] marked = {false};
                Node succ = curr.next.get(marked);
                if (marked[0]) {
                    Node nextSucc = succ;
                    if (!pred.next.compareAndSet(curr, nextSucc, false, false)) {
                        break;
                    }
                    curr = nextSucc;
                } else {
                    if (curr.key != key) {
                        pred = curr;
                        curr = succ;
                        continue;
                    }
                    if (curr.next.compareAndSet(succ, succ, false, true)) {
                        // Node has been marked
                        pred.next.compareAndSet(curr, succ, false, false);
                        size.decrementAndGet();
                        return true;
                    }
                }
            }
            return false;
        }
    }

    private boolean containsInternal(int key) {
        int hash = spread(key);
        Node bucketHead = findBucket(hash);
        Node curr = bucketHead.next.getReference();
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

    @Override
    public boolean add(int key) {
        return addInternal(key);
    }

    @Override
    public boolean remove(int key) {
        return removeInternal(key);
    }

    @Override
    public boolean contains(int key) {
        return containsInternal(key);
    }
}