package com.example.Sets;
import com.example.utils.SetADT;

import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements StackADT {
    private static class Node {
        int val;
        Node next;
        Node(int v, Node n) {
            val = v;
            next = n;
        }
    }

    private final AtomicReference<Node> top = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public void push(int val) {
        Node newNode = new Node(val, null);
        while (true) {
            Node current = top.get();
            newNode.next = current;
            if (top.compareAndSet(current, newNode)) {
                return;
            }
        }
    }

    @Override
    public int pop() {
        while (true) {
            Node current = top.get();
            if (current == null) {
                return -1;
            }
            Node next = current.next;
            if (top.compareAndSet(current, next)) {
                // Pop victim point
                return current.val;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}