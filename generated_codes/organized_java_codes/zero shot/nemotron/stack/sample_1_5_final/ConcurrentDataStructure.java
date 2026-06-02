package com.example.Sets;
import java.util.concurrent.atomic.AtomicReference;
import com.example.utils.StackADT;

public class ConcurrentDataStructure implements StackADT {
    private final AtomicReference<Node> top = new AtomicReference<>(null);

    public ConcurrentDataStructure() {
    }

    @Override
    public void push(int val) {
        Node newNode = new Node(val);
        while (true) {
            Node current = top.get();
            newNode.next = current;
            if (top.compareAndSet(current, newNode)) {
                System.out.println("Element added to stack");
                break;
            }
        }
    }

    @Override
    public int pop() {
        while (true) {
            Node current = top.get();
            if (current == null) {
                System.out.println("Stack is empty");
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

    private static class Node {
        int value;
        Node next;
        Node(int value) {
            this.value = value;
        }
    }
}