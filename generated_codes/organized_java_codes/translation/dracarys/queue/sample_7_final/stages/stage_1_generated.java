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
            Node prevTail = tail.get();
            Node prevNext = prevTail.next.get();
            if (prevTail == tail.get()) {
                if (prevNext == null) {
                    if (prevTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(prevTail, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(prevTail, prevNext);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node prevHead = head.get();
            Node prevTail = tail.get();
            Node prevNext = prevHead.next.get();
            if (prevHead == head.get()) {
                if (prevHead == prevTail) {
                    if (prevNext == null) {
                        return -1; // empty
                    }
                    tail.compareAndSet(prevTail, prevNext);
                } else {
                    int val = prevNext.val;
                    if (head.compareAndSet(prevHead, prevNext)) {
                        // Dequeue victim point
                        return val;
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