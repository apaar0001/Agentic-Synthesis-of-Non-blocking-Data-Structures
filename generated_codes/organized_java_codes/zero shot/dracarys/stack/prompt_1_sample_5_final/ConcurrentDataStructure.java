package com.example.Sets;
import com.example.utils.SetADT;

public class ConcurrentDataStructure implements StackADT {
    private Node top;

    public ConcurrentDataStructure() {
        top = null;
    }

    @Override
    public void push(int val) {
        Node newNode = new Node(val);
        newNode.next = top;
        top = newNode;
    }

    @Override
    public int pop() {
        if (top == null) {
            return -1;
        }
        int val = top.value;
        top = top.next;
        return val;
    }

    @Override
    public boolean isEmpty() {
        return top == null;
    }

    private static class Node {
        int value;
        Node next;

        Node(int value) {
            this.value = value;
        }
    }
}