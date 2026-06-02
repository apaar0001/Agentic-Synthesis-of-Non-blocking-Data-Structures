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

    private final AtomicReference<Node> top;

    public ConcurrentDataStructure() {
        this.top = new AtomicReference<>(null);
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
        Node oldTop;
        Node next;
        do {
            oldTop = top.get();
            if (oldTop == null) {
                return -1;
            }
            next = oldTop.next;
        } while (!top.compareAndSet(oldTop, next));
        // Pop victim point
        return oldTop.val;
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}