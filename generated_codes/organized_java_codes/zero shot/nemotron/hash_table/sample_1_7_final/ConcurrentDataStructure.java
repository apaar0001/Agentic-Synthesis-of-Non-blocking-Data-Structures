package com.example.Sets;
import com.example.utils.SetADT;
import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

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
    private final Node[] tailNodes;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.buckets = new AtomicReference[capacity];
        this.tailNodes = new Node[capacity];
        for (int i = 0; i < capacity; i++) {
            Node head = new Node(Integer.MIN_VALUE);
            Node tail = new Node(Integer.MAX_VALUE);
            head.next.set(tail, false);
            buckets[i] = new AtomicReference<>(head);
            tailNodes[i] = tail;
        }
    }

    private int hash(int key) {
        return Math.abs(key) % capacity;
    }

    private Node[] find(int idx, int key) {
        outer:
        while (true) {
            Node pred = buckets[idx].get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ = curr.next.getReference();
                boolean marked = curr.next.isMarked();
                if (marked) {
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break outer;
                    }
                    curr = succ;
                    continue;
                }
                if (curr == tailNodes[idx]) {
                    return new Node[]{pred, curr};
                }
                boolean succMarked = succ.next.isMarked();
                if (succMarked) {
                    if (!curr.next.compareAndSet(succ, succ.next.getReference(), false, false)) {
                        break outer;
                    }
                    continue;
                }
                if (curr.key >= key) {
                    return new Node[]{pred, curr};
                }
                pred = curr;
                curr = succ;
            }
        }
        return new Node[]{null, null};
    }

    @Override
    public boolean add(int key) {
        int idx = hash(key);
        while (true) {
            Node[] predCurr = find(idx, key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(curr, false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = hash(key);
        while (true) {
            Node[] predCurr = find(idx, key);
            Node pred = predCurr[0];
            Node curr = predCurr[1];
            if (curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (!curr.next.attemptMark(succ, true)) {
                continue;
            }
            // Node has been marked
            pred.next.compareAndSet(curr, succ, false, false);
            return true;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = hash(key);
        Node[] predCurr = find(idx, key);
        Node curr = predCurr[1];
        return curr.key == key && !curr.next.isMarked();
    }
}