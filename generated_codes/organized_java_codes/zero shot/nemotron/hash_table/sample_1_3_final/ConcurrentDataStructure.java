package com.example.Sets;
import com.example.utils.SetADT;

import java.util.concurrent.atomic.AtomicMarkableReference;

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
    private final Node[] buckets;

    public ConcurrentDataStructure() {
        this.capacity = 16;
        this.buckets = new Node[capacity];
        for (int i = 0; i < capacity; i++) {
            buckets[i] = new Node(-1, null);
        }
    }

    private int index(int key) {
        return Math.abs(key) % capacity;
    }

    @Override
    public boolean add(int key) {
        int idx = index(key);
        while (true) {
            Node head = buckets[idx];
            boolean[] marked = {false};
            Node pred = head;
            Node curr = pred.next.getReference();
            pred.next.get(marked);
            while (curr != null && marked[0]) {
                Node succ = curr.next.getReference();
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    break;
                }
                curr = pred.next.getReference();
                pred.next.get(marked);
            }
            while (curr != null) {
                if (curr.key == key) {
                    return false;
                }
                pred = curr;
                curr = pred.next.getReference();
                pred.next.get(marked);
                while (curr != null && marked[0]) {
                    Node succ = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = pred.next.getReference();
                    pred.next.get(marked);
                }
            }
            Node newNode = new Node(key, head.next.getReference());
            if (head.next.compareAndSet(newNode.next.getReference(), newNode, false, false)) {
                return true;
            }
        }
    }

    @Override
    public boolean remove(int key) {
        int idx = index(key);
        while (true) {
            Node head = buckets[idx];
            boolean[] marked = {false};
            Node pred = head;
            Node curr = pred.next.getReference();
            pred.next.get(marked);
            while (curr != null && marked[0]) {
                Node succ = curr.next.getReference();
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    break;
                }
                curr = pred.next.getReference();
                pred.next.get(marked);
            }
            outer:
            while (curr != null) {
                if (curr.key == key) {
                    if (pred.next.attemptMark(curr, true)) {
                        // Node has been marked
                        Node succ = curr.next.getReference();
                        pred.next.compareAndSet(curr, succ, true, false);
                        return true;
                    } else {
                        break outer;
                    }
                }
                pred = curr;
                curr = pred.next.getReference();
                pred.next.get(marked);
                while (curr != null && marked[0]) {
                    Node succ = curr.next.getReference();
                    if (!pred.next.compareAndSet(curr, succ, false, false)) {
                        break;
                    }
                    curr = pred.next.getReference();
                    pred.next.get(marked);
                }
            }
            return false;
        }
    }

    @Override
    public boolean contains(int key) {
        int idx = index(key);
        Node head = buckets[idx];
        boolean[] marked = {false};
        Node pred = head;
        Node curr = pred.next.getReference();
        pred.next.get(marked);
        while (curr != null && marked[0]) {
            Node succ = curr.next.getReference();
            if (!pred.next.compareAndSet(curr, succ, false, false)) {
                break;
            }
            curr = pred.next.getReference();
            pred.next.get(marked);
        }
        while (curr != null) {
            if (curr.key == key) {
                return true;
            }
            pred = curr;
            curr = pred.next.getReference();
            pred.next.get(marked);
            while (curr != null && marked[0]) {
                Node succ = curr.next.getReference();
                if (!pred.next.compareAndSet(curr, succ, false, false)) {
                    break;
                }
                curr = pred.next.getReference();
                pred.next.get(marked);
            }
        }
        return false;
    }
}