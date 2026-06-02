package com.example.Sets;

import com.example.utils.StackADT;
import java.util.concurrent.atomic.AtomicReference;

public class ConcurrentDataStructure implements StackADT {
private final AtomicReference<Node> top;


private static final class Node {
    final int val;
    final Node next;

    Node(int val, Node next) {
        this.val = val;
        this.next = next;
    }
}

public ConcurrentDataStructure() {
    this.top = new AtomicReference<>(null);
}

@Override
public void push(int val) {
    Node oldTop;
    Node newTop;
    do {
        oldTop = top.get();
        newTop = new Node(val, oldTop);
    } while (!top.compareAndSet(oldTop, newTop));
}

@Override
public int pop() {
    Node oldTop;
    Node newTop;
    do {
        oldTop = top.get();
        if (oldTop == null) {
            return -1;
        }
        newTop = oldTop.next;
    } while (!top.compareAndSet(oldTop, newTop));
    // Pop victim point
    return oldTop.val;
}

@Override
public boolean isEmpty() {
    return top.get() == null;
}


}
