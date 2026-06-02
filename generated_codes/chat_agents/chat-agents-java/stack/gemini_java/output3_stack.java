package com.example.Sets;

import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements StackADT {

    private static class Node {
        final int val;
        Node next;

        Node(int val, Node next) {
            this.val = val;
            this.next = next;
        }
    }

    private final AtomicReference<Node> top;

    public ConcurrentDataStructure() {
        this.top = new AtomicReference<>(null);
    }

    @Override
    public void push(int val) {
        Node newNode = new Node(val, null);
        Node currentTop;
        do {
            currentTop = top.get();
            newNode.next = currentTop;
        } while (!top.compareAndSet(currentTop, newNode));
    }

    @Override
    public int pop() {
        Node currentTop;
        Node newTop;
        do {
            currentTop = top.get();
            if (currentTop == null) {
                return -1;
            }
            newTop = currentTop.next;
        } while (!top.compareAndSet(currentTop, newTop));
        // Pop victim point
        return currentTop.val;
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}