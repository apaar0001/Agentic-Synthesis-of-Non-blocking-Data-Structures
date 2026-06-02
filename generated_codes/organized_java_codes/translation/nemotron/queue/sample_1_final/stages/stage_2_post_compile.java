package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {

    private static class Node {
        final int val;
        final AtomicReference<Node> next;
        Node(int val) {
            this.val = val;
            this.next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public ConcurrentDataStructure() {
        Node dummy = new Node(0);
        this.head = new AtomicReference<>(dummy);
        this.tail = new AtomicReference<>(dummy);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node t = tail.get();
            Node next = t.next.get();
            if (next != null) {
                tail.compareAndSet(t, next);
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
            Node next = h.next.get();
            if (h == t) {
                if (next == null) {
                    return -1;
                }
                tail.compareAndSet(t, next);
            } else {
                if (next != null && head.compareAndSet(h, next)) {
                    // Dequeue victim point
                    return next.val;
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        Node h = head.get();
        Node t = tail.get();
        return h == t && h.next.get() == null;
    }
}