package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final int capacity;
    private final AtomicReference<Node>[] buckets;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.buckets = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new AtomicReference<>(null);
        }
    }

    private int hash(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key);
        while (true) {
            Node head = buckets[idx].get();
            Node pred = null;
            Node curr = head;
            boolean valid = true;
            while (curr != null) {
                Node next = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred == null) {
                        if (!buckets[idx].compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    } else {
                        if (!pred.next.compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    }
                    curr = (pred == null) ? buckets[idx].get() : pred.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    return false;
                }
                pred = curr;
                curr = next;
            }
            if (!valid) {
                continue;
            }
            Node newNode = new Node(key);
            newNode.next.set(head, false);
            if (buckets[idx].compareAndSet(head, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            Node head = buckets[idx].get();
            Node pred = null;
            Node curr = head;
            boolean valid = true;
            while (curr != null) {
                Node next = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred == null) {
                        if (!buckets[idx].compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    } else {
                        if (!pred.next.compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    }
                    curr = (pred == null) ? buckets[idx].get() : pred.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    if (curr.next.attemptMark(next, true)) {
                        // Node has been marked
                        if (pred == null) {
                            buckets[idx].compareAndSet(curr, next);
                        } else {
                            pred.next.compareAndSet(curr, next);
                        }
                        return true;
                    } else {
                        return false;
                    }
                }
                pred = curr;
                curr = next;
            }
            if (!valid) {
                continue;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        while (true) {
            Node head = buckets[idx].get();
            Node pred = null;
            Node curr = head;
            boolean valid = true;
            while (curr != null) {
                Node next = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (pred == null) {
                        if (!buckets[idx].compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    } else {
                        if (!pred.next.compareAndSet(curr, next)) {
                            valid = false;
                            break;
                        }
                    }
                    curr = (pred == null) ? buckets[idx].get() : pred.next.getReference();
                    continue;
                }
                if (curr.key == key) {
                    return true;
                }
                pred = curr;
                curr = next;
            }
            if (!valid) {
                continue;
            }
            return false;
        }
    }
}