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
        head = new AtomicReference<>(dummy);
        tail = new AtomicReference<>(dummy);
    }

    @Override
    public void enqueue(int val) {
        Node node = new Node(val);
        while (true) {
            Node t = tail.get();
            Node next = t.next.get();
            if (next != null) {
                // Tail is lagging, try to advance it
                tail.compareAndSet(t, next);
            } else {
                // Try to link new node at the end
                if (t.next.compareAndSet(null, node)) {
                    // Link succeeded, try to swing tail to the new node
                    tail.compareAndSet(t, node);
                    return;
                }
                // If CAS failed, retry
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node h = head.get();
            Node t = tail.get();
            Node next = h.next.get();
            if (h == head.get()) { // Ensure h is still up-to-date
                if (next == null) {
                    // Queue is empty
                    return -1;
                }
                if (h == t) {
                    // Tail is lagging, try to advance it
                    tail.compareAndSet(t, next);
                } else {
                    int value = next.val;
                    if (head.compareAndSet(h, next)) {
                        // Dequeue victim point
                        return value;
                    }
                    // If CAS failed, retry
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        Node h = head.get();
        return h.next.get() == null;
    }
}