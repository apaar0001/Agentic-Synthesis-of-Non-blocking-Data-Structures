package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.AtomicMarkableReference;

public class ConcurrentDataStructure implements SetADT {
    private final int capacity;
    private final AtomicReference<Node>[] heads;

    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.heads = new AtomicReference[capacity];
        for (int i = 0; i < capacity; i++) {
            this.heads[i] = new AtomicReference<>(null);
        }
    }

    private int bucketIndex(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int idx = bucketIndex(key);
        while (true) {
            AtomicReference<Node> headRef = heads[idx];
            Node oldHead = headRef.get();
            Node pred = null;
            Node curr = oldHead;
            boolean[] marked = {false};
            while (curr != null) {
                Node next = curr.next.get(marked);
                if (marked[0]) {
                    Node succ = curr.next.getReference();
                    if (pred == null) {
                        if (!headRef.compareAndSet(curr, succ)) break;
                    } else {
                        if (!pred.next.compareAndSet(curr, succ, false, false)) break;
                    }
                    curr = succ;
                    continue;
                }
                if (curr.key == key) {
                    return false;
                }
                pred = curr;
                curr = next;
            }
            Node newNode = new Node(key, oldHead);
            if (headRef.compareAndSet(oldHead, newNode)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = bucketIndex(key);
        while (true) {
            AtomicReference<Node> headRef = heads[idx];
            Node oldHead = headRef.get();
            Node pred = null;
            Node curr = oldHead;
            boolean[] marked = {false};
            while (curr != null) {
                Node next = curr.next.get(marked);
                if (marked[0]) {
                    Node succ = curr.next.getReference();
                    if (pred == null) {
                        if (!headRef.compareAndSet(curr, succ)) break;
                    } else {
                        if (!pred.next.compareAndSet(curr, succ, false, false)) break;
                    }
                    curr = succ;
                    continue;
                }
                if (curr.key == key) {
                    Node nextNode = curr.next.getReference();
                    if (curr.next.compareAndSet(nextNode, nextNode, false, true)) {
                        // Node has been marked
                        if (pred == null) {
                            headRef.compareAndSet(curr, nextNode);
                        } else {
                            pred.next.compareAndSet(curr, nextNode, true, false);
                        }
                        return true;
                    } else {
                        break;
                    }
                }
                pred = curr;
                curr = next;
            }
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = bucketIndex(key);
        AtomicReference<Node> headRef = heads[idx];
        Node curr = headRef.get();
        boolean[] marked = {false};
        while (curr != null) {
            Node next = curr.next.get(marked);
            if (marked[0]) {
                curr = curr.next.getReference();
                continue;
            }
            if (curr.key == key) {
                return true;
            }
            curr = next;
        }
        return false;
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