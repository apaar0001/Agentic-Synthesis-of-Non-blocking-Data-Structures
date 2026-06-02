package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {

    private static class Node {
        int val;
        AtomicReference<Node> next;

        Node(int v) {
            this.val = v;
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
        Node node = new Node(val);
        while (true) {
            Node t = tail.get();
            Node next = t.next.get();
            if (t == tail.get()) {
                if (next == null) {
                    if (t.next.compareAndSet(null, node)) {
                        tail.compareAndSet(t, node);
                        return;
                    }
                } else {
                    tail.compareAndSet(t, next);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node h = head.get();
            Node t = tail.get();
            Node next = h.next.get();
            if (h == head.get()) {
                if (h == t) {
                    if (next == null) {
                        return -1;
                    }
                    tail.compareAndSet(t, next);
                } else {
                    if (next != null) {
                        if (head.compareAndSet(h, next)) {
                            // Dequeue victim point
                            return next.val;
                        }
                    }
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return head.get().next.get() == null;
    }
}