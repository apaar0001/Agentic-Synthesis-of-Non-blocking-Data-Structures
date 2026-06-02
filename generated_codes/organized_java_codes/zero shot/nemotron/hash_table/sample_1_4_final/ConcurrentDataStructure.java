package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private final int capacity;
    private final AtomicReference<Node>[] buckets;

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node() {
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            this.buckets[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return Math.abs(key) % capacity;
    }

    private Node[] find(int key, AtomicReference<Node> head) {
        Node pred = null;
        Node curr = null;
        Node succ = null;
        boolean[] marked = {false};
        boolean snip;
        retry:
        while (true) {
            pred = head.get();
            if (pred == null) {
                curr = null;
            } else {
                curr = pred.next.getReference();
            }
            while (true) {
                if (curr != null) {
                    marked[0] = curr.next.isMarked();
                    succ = curr.next.getReference();
                    if (marked[0]) {
                        snip = (pred == null)
                                ? head.compareAndSet(pred, succ)
                                : pred.next.compareAndSet(curr, succ, false, false);
                        if (!snip) continue retry;
                        curr = succ;
                        continue;
                    }
                }
                if (curr == null || curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = succ;
            }
        }
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key);
        while (true) {
            Node[] res = find(key, buckets[idx]);
            Node pred = res[0];
            Node curr = res[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node();
            node.key = key;
            node.next = new AtomicMarkableReference<>(curr, false);
            if (pred == null) {
                if (buckets[idx].compareAndSet(curr, node)) {
                    return true;
                }
            } else {
                if (pred.next.compareAndSet(curr, node, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            Node[] res = find(key, buckets[idx]);
            Node pred = res[0];
            Node curr = res[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                if (pred == null) {
                    buckets[idx].compareAndSet(curr, succ);
                } else {
                    pred.next.compareAndSet(curr, succ, false, false);
                }
                return true;
            }
            if (pred == null) {
                buckets[idx].compareAndSet(curr, succ);
            } else {
                pred.next.compareAndSet(curr, succ, false, false);
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        Node[] res = find(key, buckets[idx]);
        Node curr = res[1];
        return (curr != null && curr.key == key && !curr.next.isMarked());
    }
}