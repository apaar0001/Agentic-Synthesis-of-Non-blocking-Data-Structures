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
        Node sentinel = new Node(-1);
        head = new AtomicReference<>(sentinel);
        tail = new AtomicReference<>(sentinel);
    }

    @Override
    public void enqueue(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node currentTail = tail.get();
            Node next = currentTail.next.get();
            if (currentTail == tail.get()) {
                if (next == null) {
                    if (currentTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(currentTail, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(currentTail, next);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node currentHead = head.get();
            Node currentTail = tail.get();
            Node first = currentHead.next.get();
            if (currentHead == head.get()) {
                if (currentHead == currentTail) {
                    if (first == null) {
                        return -1;
                    } else {
                        tail.compareAndSet(currentTail, first);
                    }
                } else {
                    if (head.compareAndSet(currentHead, first)) {
                        // Dequeue victim point
                        return first.val;
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