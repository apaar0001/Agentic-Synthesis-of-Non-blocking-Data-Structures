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
        Node dummy = new Node(-1);
        head = new AtomicReference<>(dummy);
        tail = new AtomicReference<>(dummy);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node last = tail.get();
            Node next = last.next.get();
            if (last == tail.get()) {
                if (next == null) {
                    if (last.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(last, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(last, next);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node first = head.get();
            Node last = tail.get();
            Node next = first.next.get();
            if (first == head.get()) {
                if (first == last) {
                    if (next == null) {
                        return -1;
                    }
                    tail.compareAndSet(last, next);
                } else {
                    if (head.compareAndSet(first, next)) {
                        // Dequeue victim point
                        return next.val;
                    }
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return head.get() == tail.get() && head.get().next.get() == null;
    }
}