package com.example.Sets;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Michael-Scott Lock-Free Queue.
 */
public class QueueHarrisMichael implements QueueADT {

    private static class Node {
        final int value;
        final AtomicReference<Node> next;

        Node(int val) {
            this.value = val;
            this.next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public QueueHarrisMichael() {
        Node sentinel = new Node(0);
        head = new AtomicReference<>(sentinel);
        tail = new AtomicReference<>(sentinel);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node last = tail.get();
            Node next = last.next.get();
            if (last == tail.get()) {
                if (next == null) {
                    if (last.next.compareAndSet(next, newNode)) {
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
                    if (next == null)
                        return -1;
                    tail.compareAndSet(last, next);
                } else {
                    int value = next.value;
                    // Dequeue victim point
                    if (head.compareAndSet(first, next))
                        return value;
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return head.get().next.get() == null;
    }

}
