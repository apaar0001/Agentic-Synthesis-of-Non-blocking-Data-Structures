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
    private final AtomicReference<Node>[] heads;

    @SuppressWarnings("unchecked")
    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.heads = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            heads[i] = new AtomicReference<>(null);
        }
    }

    private int bucketIndex(int key) {
        return Math.abs(key) % capacity;
    }

    private Node[] find(int index, int key) {
        Node pred = null;
        Node curr = heads[index].get();
        while (true) {
            Node succ;
            boolean marked;
            if (curr != null) {
                succ = curr.next.getReference();
                marked = curr.next.isMarked();
            } else {
                succ = null;
                marked = false;
            }
            while (curr != null && marked) {
                Node nextUnmarked = succ;
                if (pred == null) {
                    heads[index].compareAndSet(curr, nextUnmarked);
                } else {
                    pred.next.compareAndSet(curr, nextUnmarked, false, false);
                }
                curr = (pred == null) ? heads[index].get() : pred.next.getReference();
                if (curr == null) { succ = null; marked = false; break; }
                succ = curr.next.getReference();
                marked = curr.next.isMarked();
            }
            if (curr == null) break;
            if (curr.key >= key) break;
            pred = curr;
            curr = succ;
        }
        return new Node[]{pred, curr};
    }

    @Override
    public boolean add(int key) {
        while (true) {
            int idx = bucketIndex(key);
            Node[] res = find(idx, key);
            Node pred = res[0];
            Node curr = res[1];
            if (curr != null && curr.key == key) {
                return false;
            }
            Node newNode = new Node(key);
            newNode.next.set(curr, false);
            if (pred == null) {
                if (heads[idx].compareAndSet(curr, newNode)) {
                    return true;
                }
            } else {
                if (pred.next.compareAndSet(curr, newNode, false, false)) {
                    return true;
                }
            }
        }
    }

    @Override
    public boolean remove(int key) {
        while (true) {
            int idx = bucketIndex(key);
            Node[] res = find(idx, key);
            Node pred = res[0];
            Node curr = res[1];
            if (curr == null || curr.key != key) {
                return false;
            }
            Node succ = curr.next.getReference();
            if (curr.next.attemptMark(succ, true)) {
                // Node has been marked
                if (pred == null) {
                    heads[idx].compareAndSet(curr, succ);
                } else {
                    pred.next.compareAndSet(curr, succ, false, false);
                }
                return true;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = bucketIndex(key);
        Node[] res = find(idx, key);
        Node pred = res[0];
        Node curr = res[1];
        return (curr != null && curr.key == key && !curr.next.isMarked());
    }
}