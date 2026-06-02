package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements StackADT {

    private static class Node {
        int val;
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
        while (true) {
            Node old = top.get();
            Node newNode = new Node(val, old);
            if (top.compareAndSet(old, newNode)) {
                return;
            }
        }
    }

    @Override
    public int pop() {
        while (true) {
            Node old = top.get();
            if (old == null) {
                return -1;
            }
            Node next = old.next;
            if (top.compareAndSet(old, next)) {
                // Pop victim point
                return old.val;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}