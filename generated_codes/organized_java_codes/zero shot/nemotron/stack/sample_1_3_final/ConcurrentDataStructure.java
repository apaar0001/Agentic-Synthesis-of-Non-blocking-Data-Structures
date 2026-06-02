package com.example.Sets;
import java.util.concurrent.atomic.AtomicReference;
import com.example.utils.StackADT;

public class ConcurrentDataStructure implements StackADT {
    private static class Node {
        int value;
        volatile Node next;
        Node(int value) {
            this.value = value;
            this.next = null;
        }
    }

    private final AtomicReference<Node> top = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public void push(int val) {
        Node node = new Node(val);
        while (true) {
            Node current = top.get();
            node.next = current;
            if (top.compareAndSet(current, node)) {
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
                return current.value;
            }
        }
    }

    @Override
    public boolean isEmpty() {
        return top.get() == null;
    }
}