package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.QueueADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements QueueADT {
    private static class Node {
        int val;
        AtomicReference<Node> next;

        Node(int val) {
            this.val = val;
            this.next = new AtomicReference<>(null);
        }
    }

    private final AtomicReference<Node> head;
    private final AtomicReference<Node> tail;

    public ConcurrentDataStructure() {
        Node sentinel = new Node(0);
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
                        return -1;
                    }
                    tail.compareAndSet(prevTail, prevNext);
                } else {
                    int value = prevNext.val;
                    if (head.compareAndSet(prevHead, prevNext)) {
                        // Dequeue victim point
                        return value;
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