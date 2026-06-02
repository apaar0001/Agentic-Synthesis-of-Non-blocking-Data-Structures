package com.example.Sets;
import java.util.concurrent.atomic.AtomicReference;
import com.example.utils.QueueADT;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        int val;
        AtomicReference<Node> next;
        Node(int v) {
            val = v;
            next = new AtomicReference<>(null);
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
        Node n = new Node(val);
        while (true) {
            Node t = tail.get();
            Node next = t.next.get();
            if (next == null) {
                if (t.next.compareAndSet(null, n)) {
                    tail.compareAndSet(t, n);
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
            if (h == head.get()) {
                if (t.next.get() != null) {
                    tail.compareAndSet(t, t.next.get());
                }
                if (h == t) {
                    if (next == null) {
                        return -1;
                    }
                    continue;
                } else {
                    int v = next.val;
                    if (head.compareAndSet(h, next)) {
                        // Dequeue victim point
                        return v;
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