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

    private AtomicReference<Node> head;
    private AtomicReference<Node> tail;

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
            Node currentNext = currentTail.next.get();
            if (currentTail == tail.get()) {
                if (currentNext == null) {
                    if (currentTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(currentTail, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(currentTail, currentNext);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node currentHead = head.get();
            Node currentTail = tail.get();
            Node currentNext = currentHead.next.get();
            if (currentHead == head.get()) {
                if (currentHead == currentTail) {
                    if (currentNext == null) {
                        return -1; // empty
                    }
                    tail.compareAndSet(currentTail, currentNext);
                } else {
                    int val = currentNext.val;
                    if (head.compareAndSet(currentHead, currentNext)) {
                        // Dequeue victim point
                        return val;
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