package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private static final int DEFAULT_CAPACITY = 16;
    private final int capacity;
    @SuppressWarnings("unchecked")
    private final AtomicReference<Node>[] buckets;

    private static class Node {
        int key;
        final AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    public ConcurrentDataStructure() {
        this.capacity = DEFAULT_CAPACITY;
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
            AtomicReference<Node> headRef = buckets[idx];
            Node pred = null;
            Node curr = null;
            while (true) {
                pred = headRef.get();
                if (pred == null) {
                    curr = null;
                } else {
                    curr = pred.next.getReference();
                }
                boolean valid = true;
                while (curr != null) {
                    Node succ = curr.next.getReference();
                    if (curr.next.isMarked()) {
                        if (pred == null) {
                            if (!headRef.compareAndSet(pred, succ)) {
                                valid = false;
                                break;
                            }
                        } else {
                            if (!pred.next.compareAndSet(curr, succ, false, false)) {
                                valid = false;
                                break;
                            }
                        }
                        curr = succ;
                        continue;
                    }
                    if (curr.key >= key) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
                if (!valid) {
                    break;
                }
                if (curr != null && curr.key == key) {
                    return false;
                }
                Node node = new Node(key);
                node.next.set(curr, false);
                if (pred == null) {
                    if (headRef.compareAndSet(null, node)) {
                        return true;
                    }
                } else {
                    if (pred.next.compareAndSet(curr, node, false, false)) {
                        return true;
                    }
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            AtomicReference<Node> headRef = buckets[idx];
            Node pred = null;
            Node curr = null;
            while (true) {
                pred = headRef.get();
                if (pred == null) {
                    curr = null;
                } else {
                    curr = pred.next.getReference();
                }
                boolean valid = true;
                while (curr != null) {
                    Node succ = curr.next.getReference();
                    if (curr.next.isMarked()) {
                        if (pred == null) {
                            if (!headRef.compareAndSet(pred, succ)) {
                                valid = false;
                                break;
                            }
                        } else {
                            if (!pred.next.compareAndSet(curr, succ, false, false)) {
                                valid = false;
                                break;
                            }
                        }
                        curr = succ;
                        continue;
                    }
                    if (curr.key >= key) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
                if (!valid) {
                    break;
                }
                if (curr == null || curr.key != key) {
                    return false;
                }
                Node succ = curr.next.getReference();
                if (curr.next.attemptMark(succ, true)) {
                    // Node has been marked
                    return true;
                }
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        while (true) {
            AtomicReference<Node> headRef = buckets[idx];
            Node pred = null;
            Node curr = null;
            while (true) {
                pred = headRef.get();
                if (pred == null) {
                    curr = null;
                } else {
                    curr = pred.next.getReference();
                }
                boolean valid = true;
                while (curr != null) {
                    Node succ = curr.next.getReference();
                    if (curr.next.isMarked()) {
                        if (pred == null) {
                            if (!headRef.compareAndSet(pred, succ)) {
                                valid = false;
                                break;
                            }
                        } else {
                            if (!pred.next.compareAndSet(curr, succ, false, false)) {
                                valid = false;
                                break;
                            }
                        }
                        curr = succ;
                        continue;
                    }
                    if (curr.key >= key) {
                        break;
                    }
                    pred = curr;
                    curr = succ;
                }
                if (!valid) {
                    break;
                }
                if (curr == null || curr.key != key) {
                    return false;
                }
                return true;
            }
        }
    }
}