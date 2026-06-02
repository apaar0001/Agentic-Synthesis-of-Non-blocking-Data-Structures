package com.example.Sets;
import com.example.utils.QueueADT;

import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        int value;
        AtomicReference<Node> next;
        Node(int v) { value = v; next = new AtomicReference<>(null); }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public ConcurrentDataStructure() {
        Node sentinel = new Node(-1);
        head = new AtomicReference<>(sentinel);
        tail = new AtomicReference<>(sentinel);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node t = tail.get();
            Node n = t.next.get();
            if (n != null) {
                tail.compareAndSet(t, n);
                continue;
            }
            if (t.next.compareAndSet(null, newNode)) {
                tail.compareAndSet(t, newNode);
                return;
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node h = head.get();
            Node t = tail.get();
            Node n = h.next.get();
            if (h == t) {
                if (n == null) {
                    return -1;
                }
                tail.compareAndSet(t, n);
                continue;
            }
            int v = n.value;
            if (head.compareAndSet(h, n)) {
                // Dequeue victim point
                return v;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return head.get().next.get() == null;
    }
}