package com.example.Sets;

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
            Node curTail = tail.get();
            Node tailNext = curTail.next.get();
            if (curTail == tail.get()) {
                if (tailNext == null) {
                    if (curTail.next.compareAndSet(null, newNode)) {
                        tail.compareAndSet(curTail, newNode);
                        return;
                    }
                } else {
                    tail.compareAndSet(curTail, tailNext);
                }
            }
        }
    }

    @Override
    public int dequeue() {
        while (true) {
            Node curHead = head.get();
            Node curTail = tail.get();
            Node headNext = curHead.next.get();
            if (curHead == head.get()) {
                if (curHead == curTail) {
                    if (headNext == null) {
                        return -1;
                    }
                    tail.compareAndSet(curTail, headNext);
                } else {
                    int val = headNext.val;
                    if (head.compareAndSet(curHead, headNext)) {
                        // Dequeue victim point
                        return val;
                    }
                }
            }
        }
    }

    @Override
    public boolean isEmpty() {
        Node curHead = head.get();
        Node curTail = tail.get();
        Node headNext = curHead.next.get();
        return curHead == curTail && headNext == null;
    }
}