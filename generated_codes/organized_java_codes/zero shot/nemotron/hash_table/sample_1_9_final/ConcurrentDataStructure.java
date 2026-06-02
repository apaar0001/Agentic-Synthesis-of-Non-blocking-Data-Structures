package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements SetADT {

    private static class Node {
        int key;
        final AtomicMarkableReference<Node> next;
        Node(int key) {
            this.key = key;
            this.next = new AtomicMarkableReference<>(null, false);
        }
    }

    private final int capacity;
    private final AtomicReference<Node>[] heads;

    public ConcurrentDataStructure() {
        this.capacity = 16;
        AtomicReference<Node>[] tmp = new AtomicReference[capacity];
        this.heads = tmp;
        for (int i = 0; i < capacity; i++) {
            Node sentinel = new Node(0);
            heads[i] = new AtomicReference<>(sentinel);
        }
    }

    private int index(int key) {
        return Math.abs(key) % capacity;
    }

    private Node[] find(int key, AtomicReference<Node> head) {
        while (true) {
            Node pred = head.get();
            Node curr = pred.next.getReference();
            while (true) {
                Node succ;
                boolean marked;
                while (curr != null && (marked = curr.next.isMarked())) {
                    succ = curr.next.getReference();
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                    } else {
                        break;
                    }
                    curr = succ;
                }
                if (curr == null) {
                    return new Node[]{pred, null};
                }
                succ = curr.next.getReference();
                marked = curr.next.isMarked();
                if (marked) {
                    if (pred.next.compareAndSet(curr, succ, false, false)) {
                    } else {
                        break;
                    }
                    curr = succ;
                    continue;
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
            Node[] res = find(key, heads[idx]);
            Node pred = res[0];
            Node curr = res[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node node = new Node(key);
            node.next.set(pred.next.getReference(), false);
            if (pred.next.compareAndSet(curr, node, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = index(key);
        while (true) {
            Node[] res = find(key, heads[idx]);
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
        Node curr = heads[idx].get().next.getReference();
        while (curr != null) {
            boolean marked = curr.next.isMarked();
            if (marked) {
                curr = curr.next.getReference();
                continue;
            }
            if (curr.key == key) {
                return true;
            }
            if (curr.key > key) {
                return false;
            }
            curr = curr.next.getReference();
        }
        return false;
    }
}