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
        Node() {
            this.val = 0;
            this.next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public ConcurrentDataStructure() {
        Node dummy = new Node();
        head = new AtomicReference<>(dummy);
        tail = new AtomicReference<>(dummy);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node t = tail.get();
            Node next = t.next.get();
            if (next == null) {
                if (t.next.compareAndSet(null, newNode)) {
                    tail.compareAndSet(t, newNode);
                    return;
                }
            } else {
                tail.compareAndSet(t, next);
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node h = head.get();
            Node t = tail.get();
            Node next = h.next.get();
            if (h != head.get()) {
                continue;
            }
            if (h == t) {
                if (next == null) {
                    return -1;
                }
                tail.compareAndSet(t, next);
            } else {
                int value = next.val;
                if (head.compareAndSet(h, next)) {
                    // Dequeue victim point
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