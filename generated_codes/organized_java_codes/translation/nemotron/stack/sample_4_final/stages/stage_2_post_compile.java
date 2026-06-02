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

    private final AtomicReference<Node> top = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public void push(int val) {
        Node newNode;
        Node oldTop;
        do {
            oldTop = top.get();
            newNode = new Node(val, oldTop);
        } while (!top.compareAndSet(oldTop, newNode));
    }

    @Override
    public int pop() {
        while (true) {
            Node oldTop = top.get();
            if (oldTop == null) {
                return -1;
            }
            Node next = oldTop.next;
            if (top.compareAndSet(oldTop, next)) {
                // Pop victim point
                return oldTop.val;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}