package com.example.Sets;

import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16; // power of two
    private final int capacity;
    private final AtomicReference<Node>[] buckets;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
        this.buckets = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            Node head = new Node(Integer.MIN_VALUE, null);
            Node tail = new Node(Integer.MAX_VALUE, null);
            head.next.set(tail, false);
            this.buckets[i] = new AtomicReference<>(head);
        }
    }

    private int hash(int key) {
        return (key & 0x7fffffff) & (capacity - 1);
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key);
        while (true) {
            Node pred = buckets[idx].get();
            Node curr = pred.next.getReference();
            while (true) {
                boolean marked = curr.next.isMarked();
                if (marked) {
                    Node succ = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        // helped unlink
                    }
                    curr = succ;
                    continue;
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = curr.next.getReference();
            }
            if (curr.key == key) {
                return false; // already present
            }
            Node node = new Node(key, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
            // else retry
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            Node pred = buckets[idx].get();
            Node curr = pred.next.getReference();
            while (true) {
                boolean marked = curr.next.isMarked();
                if (marked) {
                    Node succ = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        // helped unlink
                    }
                    curr = succ;
                    continue;
                }
                if (curr.key >= key) {
                    break;
                }
                pred = curr;
                curr = curr.next.getReference();
            }
            if (curr.key == key) {
                if (curr.next.attemptMark(curr, true)) {
                    // Node has been marked
                    pred.next.compareAndSet(curr, curr.next.getReference(), false, false);
                    return true;
                }
                // marking failed, retry outer loop
                break;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        Node pred = buckets[idx].get();
        Node curr = pred.next.getReference();
        while (true) {
            boolean marked = curr.next.isMarked();
            if (marked) {
                Node succ = curr.next.getReference();
                if (pred.next.compareAndSet(curr, succ, false, false)) {
                    // helped unlink
                }
                curr = succ;
                continue;
            }
            if (curr.key >= key) {
                break;
            }
            pred = curr;
            curr = curr.next.getReference();
        }
        return (curr.key == key && !curr.next.isMarked());
    }

    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;

        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }
}