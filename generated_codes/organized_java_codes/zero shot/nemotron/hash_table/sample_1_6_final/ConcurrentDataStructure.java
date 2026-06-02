package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {
    private static class Node {
        int key;
        AtomicMarkableReference<Node> next;
        Node(int key, Node next) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(next, false);
        }
    }

    private final int capacity;
    private final AtomicReference<Node>[] heads;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this(16);
    }

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure(int capacity) {
        this.capacity = capacity;
        this.heads = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            Node sentinel = new Node(Integer.MIN_VALUE, null);
            this.heads[i] = new AtomicReference<>(sentinel);
        }
    }

    private int index(int key) {
        return Math.abs(key) % capacity;
    }

    private Node[] find(int idx, int key) {
        AtomicReference<Node> headRef = heads[idx];
        while (true) {
            Node pred = headRef.get();
            Node curr = pred.next.getReference();
            while (true) {
                if (curr == null) {
                    return new Node[]{pred, null};
                }
                boolean predMarked = pred.next.isMarked();
                if (predMarked) {
                    Node succ = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, true, false)) {
                        curr = succ;
                        continue;
                    } else {
                        break;
                    }
                }
                boolean currMarked = curr.next.isMarked();
                if (currMarked) {
                    Node succ = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                        curr = succ;
                        continue;
                    } else {
                        break;
                    }
                }
                if (curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = curr.next.getReference();
            }
        }
    }

    @Override
    public boolean add(int key) {
        int idx = index(key);
        while (true) {
            Node[] res = find(idx, key);
            Node pred = res[0];
            Node curr = res[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node(key, curr);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = index(key);
        while (true) {
            Node[] res = find(idx, key);
            Node pred = res[0];
            Node curr = res[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                pred.next.compareAndSet(curr, succ, false, false);
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = index(key);
        Node[] res = find(idx, key);
        Node pred = res[0];
        Node curr = res[1];
        return curr != null && curr.key == key && !curr.next.isMarked();
    }
}