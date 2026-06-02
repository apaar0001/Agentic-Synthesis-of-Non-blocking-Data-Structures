package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicReference<Node> next;
        AtomicBoolean marked;
        Node(int key) {
            this.key = key;
            this.next = new AtomicReference<>(null);
            this.marked = new AtomicBoolean(false);
        }
    }

    private final Node[] heads;
    private final int capacity;

    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.heads = new Node[capacity];
        for (int i = 0; i < capacity; i++) {
            this.heads[i] = null;
        }
    }

    private int hash(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key);
        while (true) {
            Node pred = null;
            Node curr = heads[idx];
            boolean inserted = false;
            while (true) {
                if (curr == null) {
                    break;
                }
                boolean marked = curr.marked.get();
                if (marked) {
                    Node succ = curr.next.get();
                    if (pred == null) {
                        if (!heads[idx].compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    } else {
                        if (!pred.next.compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    }
                }
                if (curr.key == key) {
                    return false;
                }
                pred = curr;
                curr = curr.next.get();
            }
            Node node = new Node(key);
            node.next.set(pred == null ? heads[idx] : (pred != null ? pred.next.get() : null));
            if (pred == null) {
                if (heads[idx].compareAndSet(node.next.get(), node)) {
                    inserted = true;
                }
            } else {
                if (pred.next.compareAndSet(node.next.get(), node)) {
                    inserted = true;
                }
            }
            if (inserted) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            Node pred = null;
            Node curr = heads[idx];
            boolean found = false;
            while (true) {
                if (curr == null) {
                    break;
                }
                boolean marked = curr.marked.get();
                if (marked) {
                    Node succ = curr.next.get();
                    if (pred == null) {
                        if (!heads[idx].compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    } else {
                        if (!pred.next.compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    }
                }
                if (curr.key == key) {
                    found = true;
                    break;
                }
                pred = curr;
                curr = curr.next.get();
            }
            if (!found) {
                return false;
            }
            if (curr.marked.compareAndSet(false, true)) {
                // Node has been marked
                return true;
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        while (true) {
            Node pred = null;
            Node curr = heads[idx];
            while (true) {
                if (curr == null) {
                    return false;
                }
                boolean marked = curr.marked.get();
                if (marked) {
                    Node succ = curr.next.get();
                    if (pred == null) {
                        if (!heads[idx].compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    } else {
                        if (!pred.next.compareAndSet(curr, succ)) {
                            break;
                        }
                        curr = succ;
                        continue;
                    }
                }
                if (curr.key == key) {
                    return true;
                }
                pred = curr;
                curr = curr.next.get();
            }
        }
    }
}